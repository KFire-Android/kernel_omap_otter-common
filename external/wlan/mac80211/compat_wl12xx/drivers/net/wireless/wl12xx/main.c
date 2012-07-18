/*
 * This file is part of wl1271
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/crc32.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/wl12xx.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include "wl12xx.h"
#include "debug.h"
#include "wl12xx_80211.h"
#include "reg.h"
#include "io.h"
#include "event.h"
#include "tx.h"
#include "rx.h"
#include "ps.h"
#include "init.h"
#include "debugfs.h"
#include "cmd.h"
#include "boot.h"
#include "testmode.h"
#include "scan.h"
#include "version.h"

#define WL1271_BOOT_RETRIES 3

static struct conf_drv_settings default_conf = {
	.sg = {
		.params = {
			[CONF_SG_ACL_BT_MASTER_MIN_BR] = 10,
			[CONF_SG_ACL_BT_MASTER_MAX_BR] = 180,
			[CONF_SG_ACL_BT_SLAVE_MIN_BR] = 10,
			[CONF_SG_ACL_BT_SLAVE_MAX_BR] = 180,
			[CONF_SG_ACL_BT_MASTER_MIN_EDR] = 10,
			[CONF_SG_ACL_BT_MASTER_MAX_EDR] = 80,
			[CONF_SG_ACL_BT_SLAVE_MIN_EDR] = 10,
			[CONF_SG_ACL_BT_SLAVE_MAX_EDR] = 80,
			[CONF_SG_ACL_WLAN_PS_MASTER_BR] = 8,
			[CONF_SG_ACL_WLAN_PS_SLAVE_BR] = 8,
			[CONF_SG_ACL_WLAN_PS_MASTER_EDR] = 20,
			[CONF_SG_ACL_WLAN_PS_SLAVE_EDR] = 20,
			[CONF_SG_ACL_WLAN_ACTIVE_MASTER_MIN_BR] = 20,
			[CONF_SG_ACL_WLAN_ACTIVE_MASTER_MAX_BR] = 35,
			[CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MIN_BR] = 16,
			[CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MAX_BR] = 35,
			[CONF_SG_ACL_WLAN_ACTIVE_MASTER_MIN_EDR] = 32,
			[CONF_SG_ACL_WLAN_ACTIVE_MASTER_MAX_EDR] = 50,
			[CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MIN_EDR] = 28,
			[CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MAX_EDR] = 50,
			[CONF_SG_ACL_ACTIVE_SCAN_WLAN_BR] = 10,
			[CONF_SG_ACL_ACTIVE_SCAN_WLAN_EDR] = 20,
			[CONF_SG_ACL_PASSIVE_SCAN_BT_BR] = 75,
			[CONF_SG_ACL_PASSIVE_SCAN_WLAN_BR] = 15,
			[CONF_SG_ACL_PASSIVE_SCAN_BT_EDR] = 27,
			[CONF_SG_ACL_PASSIVE_SCAN_WLAN_EDR] = 17,
			/* active scan params */
			[CONF_SG_AUTO_SCAN_PROBE_REQ] = 170,
			[CONF_SG_ACTIVE_SCAN_DURATION_FACTOR_HV3] = 50,
			[CONF_SG_ACTIVE_SCAN_DURATION_FACTOR_A2DP] = 100,
			/* passive scan params */
			[CONF_SG_PASSIVE_SCAN_DURATION_FACTOR_A2DP_BR] = 800,
			[CONF_SG_PASSIVE_SCAN_DURATION_FACTOR_A2DP_EDR] = 200,
			[CONF_SG_PASSIVE_SCAN_DURATION_FACTOR_HV3] = 200,
			/* passive scan in dual antenna params */
			[CONF_SG_CONSECUTIVE_HV3_IN_PASSIVE_SCAN] = 0,
			[CONF_SG_BCN_HV3_COLLISION_THRESH_IN_PASSIVE_SCAN] = 0,
			[CONF_SG_TX_RX_PROTECTION_BWIDTH_IN_PASSIVE_SCAN] = 0,
			/* general params */
			[CONF_SG_STA_FORCE_PS_IN_BT_SCO] = 1,
			[CONF_SG_ANTENNA_CONFIGURATION] = 0,
			[CONF_SG_BEACON_MISS_PERCENT] = 60,
			[CONF_SG_DHCP_TIME] = 5000,
			[CONF_SG_RXT] = 1200,
			[CONF_SG_TXT] = 1000,
			[CONF_SG_ADAPTIVE_RXT_TXT] = 1,
			[CONF_SG_GENERAL_USAGE_BIT_MAP] = 3,
			[CONF_SG_HV3_MAX_SERVED] = 6,
			[CONF_SG_PS_POLL_TIMEOUT] = 10,
			[CONF_SG_UPSD_TIMEOUT] = 10,
			[CONF_SG_CONSECUTIVE_CTS_THRESHOLD] = 2,
			[CONF_SG_STA_RX_WINDOW_AFTER_DTIM] = 5,
			[CONF_SG_STA_CONNECTION_PROTECTION_TIME] = 30,
			/* AP params */
			[CONF_AP_BEACON_MISS_TX] = 3,
			[CONF_AP_RX_WINDOW_AFTER_BEACON] = 10,
			[CONF_AP_BEACON_WINDOW_INTERVAL] = 2,
			[CONF_AP_CONNECTION_PROTECTION_TIME] = 0,
			[CONF_AP_BT_ACL_VAL_BT_SERVE_TIME] = 25,
			[CONF_AP_BT_ACL_VAL_WL_SERVE_TIME] = 25,
			/* CTS Diluting params */
			[CONF_SG_CTS_DILUTED_BAD_RX_PACKETS_TH] = 0,
			[CONF_SG_CTS_CHOP_IN_DUAL_ANT_SCO_MASTER] = 0,
		},
		.state = CONF_SG_PROTECTIVE,
	},
	.rx = {
		.rx_msdu_life_time           = 512000,
		.packet_detection_threshold  = 0,
		.ps_poll_timeout             = 15,
		.upsd_timeout                = 15,
		.rts_threshold               = IEEE80211_MAX_RTS_THRESHOLD,
		.rx_cca_threshold            = 0,
		.irq_blk_threshold           = 0xFFFF,
		.irq_pkt_threshold           = 0,
		.irq_timeout                 = 600,
		.queue_type                  = CONF_RX_QUEUE_TYPE_LOW_PRIORITY,
	},
	.tx = {
		.tx_energy_detection         = 0,
		.sta_rc_conf                 = {
			.enabled_rates       = 0,
			.short_retry_limit   = 10,
			.long_retry_limit    = 10,
			.aflags              = 0,
		},
		.ac_conf_count               = 4,
		.ac_conf                     = {
			[CONF_TX_AC_BE] = {
				.ac          = CONF_TX_AC_BE,
				.cw_min      = 15,
				.cw_max      = 63,
				.aifsn       = 3,
				.tx_op_limit = 0,
			},
			[CONF_TX_AC_BK] = {
				.ac          = CONF_TX_AC_BK,
				.cw_min      = 15,
				.cw_max      = 63,
				.aifsn       = 7,
				.tx_op_limit = 0,
			},
			[CONF_TX_AC_VI] = {
				.ac          = CONF_TX_AC_VI,
				.cw_min      = 15,
				.cw_max      = 63,
				.aifsn       = CONF_TX_AIFS_PIFS,
				.tx_op_limit = 3008,
			},
			[CONF_TX_AC_VO] = {
				.ac          = CONF_TX_AC_VO,
				.cw_min      = 15,
				.cw_max      = 63,
				.aifsn       = CONF_TX_AIFS_PIFS,
				.tx_op_limit = 1504,
			},
		},
		.max_tx_retries = 100,
		.ap_aging_period = 300,
		.tid_conf_count = 4,
		.tid_conf = {
			[CONF_TX_AC_BE] = {
				.queue_id    = CONF_TX_AC_BE,
				.channel_type = CONF_CHANNEL_TYPE_EDCF,
				.tsid        = CONF_TX_AC_BE,
				.ps_scheme   = CONF_PS_SCHEME_LEGACY,
				.ack_policy  = CONF_ACK_POLICY_LEGACY,
				.apsd_conf   = {0, 0},
			},
			[CONF_TX_AC_BK] = {
				.queue_id    = CONF_TX_AC_BK,
				.channel_type = CONF_CHANNEL_TYPE_EDCF,
				.tsid        = CONF_TX_AC_BK,
				.ps_scheme   = CONF_PS_SCHEME_LEGACY,
				.ack_policy  = CONF_ACK_POLICY_LEGACY,
				.apsd_conf   = {0, 0},
			},
			[CONF_TX_AC_VI] = {
				.queue_id    = CONF_TX_AC_VI,
				.channel_type = CONF_CHANNEL_TYPE_EDCF,
				.tsid        = CONF_TX_AC_VI,
				.ps_scheme   = CONF_PS_SCHEME_LEGACY,
				.ack_policy  = CONF_ACK_POLICY_LEGACY,
				.apsd_conf   = {0, 0},
			},
			[CONF_TX_AC_VO] = {
				.queue_id    = CONF_TX_AC_VO,
				.channel_type = CONF_CHANNEL_TYPE_EDCF,
				.tsid        = CONF_TX_AC_VO,
				.ps_scheme   = CONF_PS_SCHEME_LEGACY,
				.ack_policy  = CONF_ACK_POLICY_LEGACY,
				.apsd_conf   = {0, 0},
			},
		},
		.frag_threshold              = IEEE80211_MAX_FRAG_THRESHOLD,
		.tx_compl_timeout            = 700,
		.tx_compl_threshold          = 4,
		.basic_rate                  = CONF_HW_BIT_RATE_1MBPS,
		.basic_rate_5                = CONF_HW_BIT_RATE_6MBPS,
		.tmpl_short_retry_limit      = 10,
		.tmpl_long_retry_limit       = 10,
	},
	.conn = {
		.wake_up_event               = CONF_WAKE_UP_EVENT_DTIM,
		.listen_interval             = 1,
		.suspend_wake_up_event       = CONF_WAKE_UP_EVENT_DTIM,
		.suspend_listen_interval     = 1,
		.bcn_filt_mode               = CONF_BCN_FILT_MODE_ENABLED,
		.bcn_filt_ie_count           = 3,
		.bcn_filt_ie = {
			[0] = {
				.ie          = WLAN_EID_CHANNEL_SWITCH,
				.rule        = CONF_BCN_RULE_PASS_ON_APPEARANCE,
			},
			[1] = {
				.ie          = WLAN_EID_HT_INFORMATION,
				.rule        = CONF_BCN_RULE_PASS_ON_CHANGE,
			},
			[2] = {
				.ie	     = WLAN_EID_ERP_INFO,
				.rule	     = CONF_BCN_RULE_PASS_ON_CHANGE,
			},

		},
		.synch_fail_thold            = 12,
		.bss_lose_timeout            = 400,
		.cons_bcn_loss_time          = 5000,
		.max_bcn_loss_time           = 10000,
		.beacon_rx_timeout           = 10000,
		.broadcast_timeout           = 20000,
		.rx_broadcast_in_ps          = 1,
		.ps_poll_threshold           = 10,
		.bet_enable                  = CONF_BET_MODE_ENABLE,
		.bet_max_consecutive         = 50,
		.psm_entry_retries           = 8,
		.psm_exit_retries            = 16,
		.psm_entry_nullfunc_retries  = 3,
		.dynamic_ps_timeout          = 1500,
		.forced_ps                   = false,
		.keep_alive_interval         = 55000,
		.max_listen_interval         = 20,
		.elp_timeout                 = 200,
	},
	.itrim = {
		.enable = false,
		.timeout = 50000,
	},
	.pm_config = {
		.host_clk_settling_time = 5000,
		.host_fast_wakeup_support = false
	},
	.roam_trigger = {
		.trigger_pacing               = 1,
		.avg_weight_rssi_beacon       = 20,
		.avg_weight_rssi_data         = 10,
		.avg_weight_snr_beacon        = 20,
		.avg_weight_snr_data          = 10,
	},
	.scan = {
		.min_dwell_time_active        = 7500,
		.max_dwell_time_active        = 30000,
		.min_dwell_time_passive       = 100000,
		.max_dwell_time_passive       = 100000,
		.num_probe_reqs               = 2,
		.split_scan_timeout           = 50000,
	},
	.sched_scan = {
		/*
		 * Values are in TU/1000 but since sched scan FW command
		 * params are in TUs rounding up may occur.
		 */
		.base_dwell_time              = 7500,
		.max_dwell_time_delta         = 22500,
		/* based on 250bits per probe @1Mbps */
		.dwell_time_delta_per_probe   = 2000,
		/* based on 250bits per probe @6Mbps (plus a bit more) */
		.dwell_time_delta_per_probe_5 = 350,
		.dwell_time_passive           = 100000,
		.dwell_time_dfs               = 150000,
		.num_probe_reqs               = 2,
		.rssi_threshold               = -90,
		.snr_threshold                = 0,
	},
	.rf = {
		.tx_per_channel_power_compensation_2 = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		.tx_per_channel_power_compensation_5 = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
	},
	.ht = {
		.rx_ba_win_size = 8,
		.tx_ba_win_size = 64,
		.inactivity_timeout = 10000,
		.tx_ba_tid_bitmap = CONF_TX_BA_ENABLED_TID_BITMAP,
	},
	.mem_wl127x = {
		.num_stations                 = 1,
		.ssid_profiles                = 1,
		.rx_block_num                 = 70,
		.tx_min_block_num             = 40,
		.dynamic_memory               = 1,
		.min_req_tx_blocks            = 100,
		.min_req_rx_blocks            = 22,
		.tx_min                       = 27,
	},
	.mem_wl128x = {
		.num_stations                 = 1,
		.ssid_profiles                = 1,
		.rx_block_num                 = 40,
		.tx_min_block_num             = 40,
		.dynamic_memory               = 1,
		.min_req_tx_blocks            = 45,
		.min_req_rx_blocks            = 22,
		.tx_min                       = 27,
	},
	.fm_coex = {
		.enable                       = true,
		.swallow_period               = 5,
		.n_divider_fref_set_1         = 0xff,       /* default */
		.n_divider_fref_set_2         = 12,
		.m_divider_fref_set_1         = 0xffff,
		.m_divider_fref_set_2         = 148,	    /* default */
		.coex_pll_stabilization_time  = 0xffffffff, /* default */
		.ldo_stabilization_time       = 0xffff,     /* default */
		.fm_disturbed_band_margin     = 0xff,       /* default */
		.swallow_clk_diff             = 0xff,       /* default */
	},
	.rx_streaming = {
		.duration                      = 150,
		.queues                        = 0x1,
		.interval                      = 20,
		.always                        = 0,
	},
	.fwlog = {
		.mode                         = WL12XX_FWLOG_ON_DEMAND,
		.mem_blocks                   = 2,
		.severity                     = 0,
		.timestamp                    = WL12XX_FWLOG_TIMESTAMP_DISABLED,
		.output                       = WL12XX_FWLOG_OUTPUT_HOST,
		.threshold                    = 0,
	},
	.hci_io_ds = HCI_IO_DS_6MA,
	.rate = {
		.rate_retry_score = 32000,
		.per_add = 8192,
		.per_th1 = 2048,
		.per_th2 = 4096,
		.max_per = 8100,
		.inverse_curiosity_factor = 5,
		.tx_fail_low_th = 4,
		.tx_fail_high_th = 10,
		.per_alpha_shift = 4,
		.per_add_shift = 13,
		.per_beta1_shift = 10,
		.per_beta2_shift = 8,
		.rate_check_up = 2,
		.rate_check_down = 12,
		.rate_retry_policy = {
			0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00,
		},
	},
	.hangover = {
		.recover_time               = 0,
		.hangover_period            = 20,
		.dynamic_mode               = 1,
		.early_termination_mode     = 1,
		.max_period                 = 20,
		.min_period                 = 1,
		.increase_delta             = 1,
		.decrease_delta             = 2,
		.quiet_time                 = 4,
		.increase_time              = 1,
		.window_size                = 16,
	},
};

static char *fwlog_param;
static bool bug_on_recovery;
static char *fref_param;
static char *tcxo_param;

static void __wl1271_op_remove_interface(struct wl1271 *wl,
					 struct ieee80211_vif *vif,
					 bool reset_tx_queues);
static void wl1271_op_stop_locked(struct wl1271 *wl);
static void wl1271_free_ap_keys(struct wl1271 *wl, struct wl12xx_vif *wlvif);

static DEFINE_MUTEX(wl_list_mutex);
static LIST_HEAD(wl_list);

static int wl1271_check_operstate(struct wl1271 *wl, struct wl12xx_vif *wlvif,
				  unsigned char operstate)
{
	int ret;

	if (operstate != IF_OPER_UP)
		return 0;

	if (test_and_set_bit(WLVIF_FLAG_STA_STATE_SENT, &wlvif->flags))
		return 0;

	ret = wl12xx_cmd_set_peer_state(wl, wlvif->sta.hlid);
	if (ret < 0)
		return ret;

	wl12xx_croc(wl, wlvif->role_id);

	wl1271_info("Association completed.");
	return 0;
}
static int wl1271_dev_notify(struct notifier_block *me, unsigned long what,
			     void *arg)
{
	struct net_device *dev = arg;
	struct wireless_dev *wdev;
	struct wiphy *wiphy;
	struct ieee80211_hw *hw;
	struct wl1271 *wl;
	struct wl1271 *wl_temp;
	struct wl12xx_vif *wlvif;
	int ret = 0;

	/* Check that this notification is for us. */
	if (what != NETDEV_CHANGE)
		return NOTIFY_DONE;

	wdev = dev->ieee80211_ptr;
	if (wdev == NULL)
		return NOTIFY_DONE;

	wiphy = wdev->wiphy;
	if (wiphy == NULL)
		return NOTIFY_DONE;

	hw = wiphy_priv(wiphy);
	if (hw == NULL)
		return NOTIFY_DONE;

	wl_temp = hw->priv;
	mutex_lock(&wl_list_mutex);
	list_for_each_entry(wl, &wl_list, list) {
		if (wl == wl_temp)
			break;
	}
	mutex_unlock(&wl_list_mutex);
	if (wl != wl_temp)
		return NOTIFY_DONE;

	mutex_lock(&wl->mutex);

	if (wl->state == WL1271_STATE_OFF)
		goto out;

	if (dev->operstate != IF_OPER_UP)
		goto out;
	/*
	 * The correct behavior should be just getting the appropriate wlvif
	 * from the given dev, but currently we don't have a mac80211
	 * interface for it.
	 */
	wl12xx_for_each_wlvif_sta(wl, wlvif) {
		struct ieee80211_vif *vif = wl12xx_wlvif_to_vif(wlvif);

		if (!test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags))
			continue;

		ret = wl1271_ps_elp_wakeup(wl);
		if (ret < 0)
			goto out;

		wl1271_check_operstate(wl, wlvif,
				       ieee80211_get_operstate(vif));

		wl1271_ps_elp_sleep(wl);
	}
out:
	mutex_unlock(&wl->mutex);

	return NOTIFY_OK;
}

static int wl1271_reg_notify(struct wiphy *wiphy,
			     struct regulatory_request *request)
{
	struct ieee80211_supported_band *band;
	struct ieee80211_channel *ch;
	int i;

	band = wiphy->bands[IEEE80211_BAND_5GHZ];
	for (i = 0; i < band->n_channels; i++) {
		ch = &band->channels[i];
		if (ch->flags & IEEE80211_CHAN_DISABLED)
			continue;

		if (ch->flags & IEEE80211_CHAN_RADAR)
			ch->flags |= IEEE80211_CHAN_NO_IBSS |
				     IEEE80211_CHAN_PASSIVE_SCAN;

	}

	return 0;
}

static int wl1271_set_rx_streaming(struct wl1271 *wl, struct wl12xx_vif *wlvif,
				   bool enable)
{
	int ret = 0;

	/* we should hold wl->mutex */
	ret = wl1271_acx_ps_rx_streaming(wl, wlvif, enable);
	if (ret < 0)
		goto out;

	if (enable)
		set_bit(WLVIF_FLAG_RX_STREAMING_STARTED, &wlvif->flags);
	else
		clear_bit(WLVIF_FLAG_RX_STREAMING_STARTED, &wlvif->flags);
out:
	return ret;
}

/*
 * this function is being called when the rx_streaming interval
 * has beed changed or rx_streaming should be disabled
 */
int wl1271_recalc_rx_streaming(struct wl1271 *wl, struct wl12xx_vif *wlvif)
{
	int ret = 0;
	int period = wl->conf.rx_streaming.interval;

	/* don't reconfigure if rx_streaming is disabled */
	if (!test_bit(WLVIF_FLAG_RX_STREAMING_STARTED, &wlvif->flags))
		goto out;

	/* reconfigure/disable according to new streaming_period */
	if (period &&
	    test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags) &&
	    (wl->conf.rx_streaming.always ||
	     test_bit(WL1271_FLAG_SOFT_GEMINI, &wl->flags)))
		ret = wl1271_set_rx_streaming(wl, wlvif, true);
	else {
		ret = wl1271_set_rx_streaming(wl, wlvif, false);
		/* don't cancel_work_sync since we might deadlock */
		del_timer_sync(&wlvif->rx_streaming_timer);
	}
out:
	return ret;
}

static void wl1271_rx_streaming_enable_work(struct work_struct *work)
{
	int ret;
	struct wl12xx_vif *wlvif = container_of(work, struct wl12xx_vif,
						rx_streaming_enable_work);
	struct wl1271 *wl = wlvif->wl;

	mutex_lock(&wl->mutex);

	if (test_bit(WLVIF_FLAG_RX_STREAMING_STARTED, &wlvif->flags) ||
	    !test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags) ||
	    (!wl->conf.rx_streaming.always &&
	     !test_bit(WL1271_FLAG_SOFT_GEMINI, &wl->flags)))
		goto out;

	if (!wl->conf.rx_streaming.interval)
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_set_rx_streaming(wl, wlvif, true);
	if (ret < 0)
		goto out_sleep;

	/* stop it after some time of inactivity */
	mod_timer(&wlvif->rx_streaming_timer,
		  jiffies + msecs_to_jiffies(wl->conf.rx_streaming.duration));

out_sleep:
	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
}

static void wl1271_rx_streaming_disable_work(struct work_struct *work)
{
	int ret;
	struct wl12xx_vif *wlvif = container_of(work, struct wl12xx_vif,
						rx_streaming_disable_work);
	struct wl1271 *wl = wlvif->wl;

	mutex_lock(&wl->mutex);

	if (!test_bit(WLVIF_FLAG_RX_STREAMING_STARTED, &wlvif->flags))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_set_rx_streaming(wl, wlvif, false);
	if (ret)
		goto out_sleep;

out_sleep:
	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
}

static void wl1271_rx_streaming_timer(unsigned long data)
{
	struct wl12xx_vif *wlvif = (struct wl12xx_vif *)data;
	struct wl1271 *wl = wlvif->wl;
	ieee80211_queue_work(wl->hw, &wlvif->rx_streaming_disable_work);
}

static void wl1271_conf_init(struct wl1271 *wl)
{

	/*
	 * This function applies the default configuration to the driver. This
	 * function is invoked upon driver load (spi probe.)
	 *
	 * The configuration is stored in a run-time structure in order to
	 * facilitate for run-time adjustment of any of the parameters. Making
	 * changes to the configuration structure will apply the new values on
	 * the next interface up (wl1271_op_start.)
	 */

	/* apply driver default configuration */
	memcpy(&wl->conf, &default_conf, sizeof(default_conf));

	/* Adjust settings according to optional module parameters */
	if (fwlog_param) {
		if (!strcmp(fwlog_param, "continuous")) {
			wl->conf.fwlog.mode = WL12XX_FWLOG_CONTINUOUS;
		} else if (!strcmp(fwlog_param, "ondemand")) {
			wl->conf.fwlog.mode = WL12XX_FWLOG_ON_DEMAND;
		} else if (!strcmp(fwlog_param, "dbgpins")) {
			wl->conf.fwlog.mode = WL12XX_FWLOG_CONTINUOUS;
			wl->conf.fwlog.output = WL12XX_FWLOG_OUTPUT_DBG_PINS;
		} else if (!strcmp(fwlog_param, "disable")) {
			wl->conf.fwlog.mem_blocks = 0;
			wl->conf.fwlog.output = WL12XX_FWLOG_OUTPUT_NONE;
		} else {
			wl1271_error("Unknown fwlog parameter %s", fwlog_param);
		}
	}

	wl->ref_clock = -1;
	if (fref_param) {
		if (!strcmp(fref_param, "19.2"))
			wl->ref_clock = WL12XX_REFCLOCK_19;
		else if (!strcmp(fref_param, "26"))
			wl->ref_clock = WL12XX_REFCLOCK_26;
		else if (!strcmp(fref_param, "26x"))
			wl->ref_clock = WL12XX_REFCLOCK_26_XTAL;
		else if (!strcmp(fref_param, "38.4"))
			wl->ref_clock = WL12XX_REFCLOCK_38;
		else if (!strcmp(fref_param, "38.4x"))
			wl->ref_clock = WL12XX_REFCLOCK_38_XTAL;
		else if (!strcmp(fref_param, "52"))
			wl->ref_clock = WL12XX_REFCLOCK_52;
		else
			wl1271_error("Invalid fref parameter %s", fref_param);
	}

	wl->tcxo_clock = -1;
	if (tcxo_param) {
		if (!strcmp(tcxo_param, "19.2"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_19_2;
		else if (!strcmp(tcxo_param, "26"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_26;
		else if (!strcmp(tcxo_param, "38.4"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_38_4;
		else if (!strcmp(tcxo_param, "52"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_52;
		else if (!strcmp(tcxo_param, "16.368"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_16_368;
		else if (!strcmp(tcxo_param, "32.736"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_32_736;
		else if (!strcmp(tcxo_param, "16.8"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_16_8;
		else if (!strcmp(tcxo_param, "33.6"))
			wl->tcxo_clock = WL12XX_TCXOCLOCK_33_6;
		else
			wl1271_error("Invalid tcxo parameter %s", tcxo_param);
	}
}

static int wl1271_plt_init(struct wl1271 *wl)
{
	int ret;

	if (wl->chip.id == CHIP_ID_1283_PG20)
		ret = wl128x_cmd_general_parms(wl);
	else
		ret = wl1271_cmd_general_parms(wl);
	if (ret < 0)
		return ret;

	if (wl->chip.id == CHIP_ID_1283_PG20)
		ret = wl128x_cmd_radio_parms(wl);
	else
		ret = wl1271_cmd_radio_parms(wl);
	if (ret < 0)
		return ret;

	if (wl->chip.id != CHIP_ID_1283_PG20) {
		ret = wl1271_cmd_ext_radio_parms(wl);
		if (ret < 0)
			return ret;
	}
	if (ret < 0)
		return ret;

	/* Chip-specific initializations */
	ret = wl1271_chip_specific_init(wl);
	if (ret < 0)
		return ret;

	ret = wl1271_acx_init_mem_config(wl);
	if (ret < 0)
		return ret;

	ret = wl12xx_acx_mem_cfg(wl);
	if (ret < 0)
		goto out_free_memmap;

	/* Enable data path */
	ret = wl1271_cmd_data_path(wl, 1);
	if (ret < 0)
		goto out_free_memmap;

	/* Configure for CAM power saving (ie. always active) */
	ret = wl1271_acx_sleep_auth(wl, WL1271_PSM_CAM);
	if (ret < 0)
		goto out_free_memmap;

	/* configure PM */
	ret = wl1271_acx_pm_config(wl);
	if (ret < 0)
		goto out_free_memmap;

	return 0;

 out_free_memmap:
	kfree(wl->target_mem_map);
	wl->target_mem_map = NULL;

	return ret;
}

static void wl12xx_irq_ps_regulate_link(struct wl1271 *wl,
					struct wl12xx_vif *wlvif,
					u8 hlid, u8 tx_pkts)
{
	bool fw_ps, single_sta;

	fw_ps = test_bit(hlid, (unsigned long *)&wl->ap_fw_ps_map);
	single_sta = (wl->active_sta_count == 1);

	/*
	 * Wake up from high level PS if the STA is asleep with too little
	 * packets in FW or if the STA is awake.
	 */
	if (!fw_ps || tx_pkts < WL1271_PS_STA_MAX_PACKETS)
		wl12xx_ps_link_end(wl, wlvif, hlid);

	/*
	 * Start high-level PS if the STA is asleep with enough blocks in FW.
	 * Make an exception if this is the only connected station. In this
	 * case FW-memory congestion is not a problem.
	 */
	else if (!single_sta && fw_ps && tx_pkts >= WL1271_PS_STA_MAX_PACKETS)
		wl12xx_ps_link_start(wl, wlvif, hlid, true);
}

static void wl12xx_irq_update_links_status(struct wl1271 *wl,
					   struct wl12xx_vif *wlvif,
					   struct wl12xx_fw_status *status)
{
	struct wl1271_link *lnk;
	u32 cur_fw_ps_map;
	u8 hlid, cnt;

	/* TODO: also use link_fast_bitmap here */

	cur_fw_ps_map = le32_to_cpu(status->link_ps_bitmap);
	if (wl->ap_fw_ps_map != cur_fw_ps_map) {
		wl1271_debug(DEBUG_PSM,
			     "link ps prev 0x%x cur 0x%x changed 0x%x",
			     wl->ap_fw_ps_map, cur_fw_ps_map,
			     wl->ap_fw_ps_map ^ cur_fw_ps_map);

		wl->ap_fw_ps_map = cur_fw_ps_map;
	}

	for_each_set_bit(hlid, wlvif->ap.sta_hlid_map, WL12XX_MAX_LINKS) {
		lnk = &wl->links[hlid];
		cnt = status->tx_lnk_free_pkts[hlid] - lnk->prev_freed_pkts;

		lnk->prev_freed_pkts = status->tx_lnk_free_pkts[hlid];
		lnk->allocated_pkts -= cnt;

		wl12xx_irq_ps_regulate_link(wl, wlvif, hlid,
					    lnk->allocated_pkts);
	}
}

static int wl12xx_fw_status(struct wl1271 *wl,
			    struct wl12xx_fw_status *status)
{
	struct wl12xx_vif *wlvif;
	struct timespec ts;
	u32 old_tx_blk_count = wl->tx_blocks_available;
	int avail, freed_blocks;
	int i;
	int ret;

	ret = wl1271_raw_read(wl, FW_STATUS_ADDR, status, sizeof(*status),
			      false);
	if (ret < 0)
		return ret;

	wl1271_debug(DEBUG_IRQ, "intr: 0x%x (fw_rx_counter = %d, "
		     "drv_rx_counter = %d, tx_results_counter = %d)",
		     status->intr,
		     status->fw_rx_counter,
		     status->drv_rx_counter,
		     status->tx_results_counter);

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		/* prevent wrap-around in freed-packets counter */
		wl->tx_allocated_pkts[i] -=
				(status->tx_released_pkts[i] -
				wl->tx_pkts_freed[i]) & 0xff;

		wl->tx_pkts_freed[i] = status->tx_released_pkts[i];
	}

	/* prevent wrap-around in total blocks counter */
	if (likely(wl->tx_blocks_freed <=
		   le32_to_cpu(status->total_released_blks)))
		freed_blocks = le32_to_cpu(status->total_released_blks) -
			       wl->tx_blocks_freed;
	else
		freed_blocks = 0x100000000LL - wl->tx_blocks_freed +
			       le32_to_cpu(status->total_released_blks);

	wl->tx_blocks_freed = le32_to_cpu(status->total_released_blks);

	wl->tx_allocated_blocks -= freed_blocks;

	avail = le32_to_cpu(status->tx_total) - wl->tx_allocated_blocks;

	/*
	 * The FW might change the total number of TX memblocks before
	 * we get a notification about blocks being released. Thus, the
	 * available blocks calculation might yield a temporary result
	 * which is lower than the actual available blocks. Keeping in
	 * mind that only blocks that were allocated can be moved from
	 * TX to RX, tx_blocks_available should never decrease here.
	 */
	wl->tx_blocks_available = max((int)wl->tx_blocks_available,
				      avail);

	/* if more blocks are available now, tx work can be scheduled */
	if (wl->tx_blocks_available > old_tx_blk_count)
		clear_bit(WL1271_FLAG_FW_TX_BUSY, &wl->flags);

	/* for AP update num of allocated TX blocks per link and ps status */
	wl12xx_for_each_wlvif_ap(wl, wlvif) {
		wl12xx_irq_update_links_status(wl, wlvif, status);
	}

	/* update the host-chipset time offset */
	getnstimeofday(&ts);
	wl->time_offset = (timespec_to_ns(&ts) >> 10) -
		(s64)le32_to_cpu(status->fw_localtime);

	return 0;
}

static void wl1271_flush_deferred_work(struct wl1271 *wl)
{
	struct sk_buff *skb;

	/* Pass all received frames to the network stack */
	while ((skb = skb_dequeue(&wl->deferred_rx_queue)))
		ieee80211_rx_ni(wl->hw, skb);

	/* Return sent skbs to the network stack */
	while ((skb = skb_dequeue(&wl->deferred_tx_queue)))
		ieee80211_tx_status_ni(wl->hw, skb);
}

static void wl1271_netstack_work(struct work_struct *work)
{
	struct wl1271 *wl =
		container_of(work, struct wl1271, netstack_work);

	do {
		wl1271_flush_deferred_work(wl);
	} while (skb_queue_len(&wl->deferred_rx_queue));
}

#define WL1271_IRQ_MAX_LOOPS 256

static int wl12xx_irq_locked(struct wl1271 *wl)
{
	int ret = 0;
	u32 intr;
	int loopcount = WL1271_IRQ_MAX_LOOPS;
	bool done = false;
	unsigned int defer_count;
	unsigned long flags;

	/*
	 * In case edge triggered interrupt must be used, we cannot iterate
	 * more than once without introducing race conditions with the hardirq.
	 */
	if (wl->platform_quirks & WL12XX_PLATFORM_QUIRK_EDGE_IRQ)
		loopcount = 1;

	wl1271_debug(DEBUG_IRQ, "IRQ work");

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	while (!done && loopcount--) {
		/*
		 * In order to avoid a race with the hardirq, clear the flag
		 * before acknowledging the chip. Since the mutex is held,
		 * wl1271_ps_elp_wakeup cannot be called concurrently.
		 */
		clear_bit(WL1271_FLAG_IRQ_RUNNING, &wl->flags);
		smp_mb__after_clear_bit();

		ret = wl12xx_fw_status(wl, wl->fw_status);
		if (ret < 0)
			goto out;

		intr = le32_to_cpu(wl->fw_status->intr);
		intr &= WL1271_INTR_MASK;
		if (!intr) {
			done = true;
			continue;
		}

		if (unlikely(intr & WL1271_ACX_INTR_WATCHDOG)) {
			wl1271_error("watchdog interrupt received! "
				     "starting recovery.");
			wl->watchdog_recovery = true;

			/* restarting the chip. ignore any other interrupt. */
			goto out;
		}

		if (likely(intr & WL1271_ACX_INTR_DATA)) {
			wl1271_debug(DEBUG_IRQ, "WL1271_ACX_INTR_DATA");

			ret = wl12xx_rx(wl, wl->fw_status);
			if (ret < 0)
				goto out;

			/* Check if any tx blocks were freed */
			spin_lock_irqsave(&wl->wl_lock, flags);
			if (!test_bit(WL1271_FLAG_FW_TX_BUSY, &wl->flags) &&
			    wl1271_tx_total_queue_count(wl) > 0) {
				spin_unlock_irqrestore(&wl->wl_lock, flags);
				/*
				 * In order to avoid starvation of the TX path,
				 * call the work function directly.
				 */
				ret = wl1271_tx_work_locked(wl);
				if (ret < 0)
					goto out;
			} else {
				spin_unlock_irqrestore(&wl->wl_lock, flags);
			}

			/* check for tx results */
			if (wl->fw_status->tx_results_counter !=
			    (wl->tx_results_count & 0xff)) {
				ret = wl1271_tx_complete(wl);
				if (ret < 0)
					goto out;
			}

			/* Make sure the deferred queues don't get too long */
			defer_count = skb_queue_len(&wl->deferred_tx_queue) +
				      skb_queue_len(&wl->deferred_rx_queue);
			if (defer_count > WL1271_DEFERRED_QUEUE_LIMIT)
				wl1271_flush_deferred_work(wl);
		}

		if (intr & WL1271_ACX_INTR_EVENT_A) {
			wl1271_debug(DEBUG_IRQ, "WL1271_ACX_INTR_EVENT_A");
			ret = wl1271_event_handle(wl, 0);
			if (ret < 0)
				goto out;
		}

		if (intr & WL1271_ACX_INTR_EVENT_B) {
			wl1271_debug(DEBUG_IRQ, "WL1271_ACX_INTR_EVENT_B");
			ret = wl1271_event_handle(wl, 1);
			if (ret < 0)
				goto out;
		}

		if (intr & WL1271_ACX_INTR_INIT_COMPLETE)
			wl1271_debug(DEBUG_IRQ,
				     "WL1271_ACX_INTR_INIT_COMPLETE");

		if (intr & WL1271_ACX_INTR_HW_AVAILABLE)
			wl1271_debug(DEBUG_IRQ, "WL1271_ACX_INTR_HW_AVAILABLE");
	}

	wl1271_ps_elp_sleep(wl);

out:
	return ret;
}

static irqreturn_t wl12xx_irq(int irq, void *cookie)
{
	int ret;
	unsigned long flags;
	struct wl1271 *wl = cookie;

	/* TX might be handled here, avoid redundant work */
	set_bit(WL1271_FLAG_TX_PENDING, &wl->flags);
	cancel_work_sync(&wl->tx_work);

	mutex_lock(&wl->mutex);

	ret = wl12xx_irq_locked(wl);
	if (ret)
		wl12xx_queue_recovery_work(wl);

	spin_lock_irqsave(&wl->wl_lock, flags);
	/* In case TX was not handled here, queue TX work */
	clear_bit(WL1271_FLAG_TX_PENDING, &wl->flags);
	if (!test_bit(WL1271_FLAG_FW_TX_BUSY, &wl->flags) &&
	    wl1271_tx_total_queue_count(wl) > 0)
		ieee80211_queue_work(wl->hw, &wl->tx_work);

#ifdef CONFIG_HAS_WAKELOCK
	if (test_and_clear_bit(WL1271_FLAG_WAKE_LOCK, &wl->flags))
		wake_unlock(&wl->wake_lock);
#endif
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	mutex_unlock(&wl->mutex);

	return IRQ_HANDLED;
}

static int wl12xx_fetch_firmware(struct wl1271 *wl, bool plt)
{
	const struct firmware *fw;
	const char *fw_name;
	enum wl12xx_fw_type fw_type;
	int ret;
	u8 open_count;

	open_count = ieee80211_get_open_count(wl->hw, NULL);
	if (plt) {
		fw_type = WL12XX_FW_TYPE_PLT;
		if (wl->chip.id == CHIP_ID_1283_PG20)
			fw_name = WL128X_PLT_FW_NAME;
		else
			fw_name	= WL127X_PLT_FW_NAME;
	} else {
		if (open_count > 1) {
			fw_type = WL12XX_FW_TYPE_MULTI;
			if (wl->chip.id == CHIP_ID_1283_PG20)
				fw_name = WL128X_FW_NAME_MULTI;
			else
				fw_name = WL127X_FW_NAME_MULTI;
		} else {
			fw_type = WL12XX_FW_TYPE_NORMAL;
			if (wl->chip.id == CHIP_ID_1283_PG20)
				fw_name = WL128X_FW_NAME_SINGLE;
			else
				fw_name = WL127X_FW_NAME_SINGLE;
		}
	}

	if (wl->saved_fw_type == fw_type)
		return 0;

	wl1271_debug(DEBUG_BOOT, "booting firmware %s", fw_name);

	ret = request_firmware(&fw, fw_name, wl->dev);

	if (ret < 0) {
		wl1271_error("could not get firmware %s: %d", fw_name, ret);
		return ret;
	}

	if (fw->size % 4) {
		wl1271_error("firmware size is not multiple of 32 bits: %zu",
			     fw->size);
		ret = -EILSEQ;
		goto out;
	}

	vfree(wl->fw);
	wl->saved_fw_type = WL12XX_FW_TYPE_NONE;
	wl->fw_len = fw->size;
	wl->fw = vmalloc(wl->fw_len);

	if (!wl->fw) {
		wl1271_error("could not allocate memory for the firmware");
		ret = -ENOMEM;
		goto out;
	}

	memcpy(wl->fw, fw->data, wl->fw_len);
	ret = 0;
	wl->saved_fw_type = fw_type;
out:
	release_firmware(fw);

	return ret;
}

static int wl1271_fetch_nvs(struct wl1271 *wl)
{
	const struct firmware *fw;
	int ret;

	ret = request_firmware(&fw, WL12XX_NVS_NAME, wl->dev);

	if (ret < 0) {
		wl1271_error("could not get nvs file %s: %d", WL12XX_NVS_NAME,
			     ret);
		return ret;
	}

	wl->nvs = kmemdup(fw->data, fw->size, GFP_KERNEL);

	if (!wl->nvs) {
		wl1271_error("could not allocate memory for the nvs file");
		ret = -ENOMEM;
		goto out;
	}

	wl->nvs_len = fw->size;

out:
	release_firmware(fw);

	return ret;
}

void wl12xx_queue_recovery_work(struct wl1271 *wl)
{
	WARN_ON(!test_bit(WL1271_FLAG_INTENDED_FW_RECOVERY, &wl->flags));

	/* Avoid a recursive recovery */
	if (!test_and_set_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS, &wl->flags)) {
		wlcore_disable_interrupts_nosync(wl);
#ifdef CONFIG_HAS_WAKELOCK
		/* give us a grace period for recovery */
		wake_lock_timeout(&wl->recovery_wake, 5 * HZ);
#endif
		ieee80211_queue_work(wl->hw, &wl->recovery_work);
	}
}

size_t wl12xx_copy_fwlog(struct wl1271 *wl, u8 *memblock, size_t maxlen)
{
	size_t len = 0;

	/* The FW log is a length-value list, find where the log end */
	while (len < maxlen) {
		if (memblock[len] == 0)
			break;
		if (len + memblock[len] + 1 > maxlen)
			break;
		len += memblock[len] + 1;
	}

	/* Make sure we have enough room */
	len = min(len, (size_t)(PAGE_SIZE - wl->fwlog_size));

	/* Fill the FW log file, consumed by the sysfs fwlog entry */
	memcpy(wl->fwlog + wl->fwlog_size, memblock, len);
	wl->fwlog_size += len;

	return len;
}

static void wl12xx_read_fwlog_panic(struct wl1271 *wl)
{
	u32 addr;
	u32 first_addr;
	u8 *block;
	int ret;

	if ((wl->quirks & WL12XX_QUIRK_FWLOG_NOT_IMPLEMENTED) ||
	    (wl->conf.fwlog.mode != WL12XX_FWLOG_ON_DEMAND) ||
	    (wl->conf.fwlog.mem_blocks == 0))
		return;

	wl1271_info("Reading FW panic log");

	block = kmalloc(WL12XX_HW_BLOCK_SIZE, GFP_KERNEL);
	if (!block)
		return;

	/* Make sure the chip is awake and the logger isn't active. */
	if (!wl1271_ps_elp_wakeup(wl)) {
		/* Do not send a stop fwlog command if the fw is hanged */
		if (!wl->watchdog_recovery)
			wl12xx_cmd_stop_fwlog(wl);
	}
	else
		goto out;

	/* Read the first memory block address */
	ret = wl12xx_fw_status(wl, wl->fw_status);
	if (ret < 0)
		goto out;

	first_addr = le32_to_cpu(wl->fw_status->log_start_addr);
	if (!first_addr)
		goto out;

	/* Traverse the memory blocks linked list */
	addr = first_addr;
	do {
		memset(block, 0, WL12XX_HW_BLOCK_SIZE);
		ret = wl1271_read_hwaddr(wl, addr, block, WL12XX_HW_BLOCK_SIZE,
					 false);
		if (ret < 0)
			goto out;

		/*
		 * Memory blocks are linked to one another. The first 4 bytes
		 * of each memory block hold the hardware address of the next
		 * one. The last memory block points to the first one.
		 */
		addr = le32_to_cpup((__le32 *)block);
		if (!wl12xx_copy_fwlog(wl, block + sizeof(addr),
				       WL12XX_HW_BLOCK_SIZE - sizeof(addr)))
			break;
	} while (addr && (addr != first_addr));

	wake_up_interruptible(&wl->fwlog_waitq);

out:
	kfree(block);
}

static void wl12xx_print_recovery(struct wl1271 *wl)
{
	u32 pc = 0;
	int ret;

	wl1271_info("Hardware recovery in progress. FW ver: %s",
		    wl->chip.fw_ver_str);

	ret = wl1271_read32(wl, SCR_PAD4, &pc);
	if (ret < 0)
		return;

	wl1271_info("pc: 0x%x", pc);
}

static void wl1271_recovery_work(struct work_struct *work)
{
	struct wl1271 *wl =
		container_of(work, struct wl1271, recovery_work);
	struct wl12xx_vif *wlvif;
	struct ieee80211_vif *vif;

	mutex_lock(&wl->mutex);

	if (wl->state != WL1271_STATE_ON)
		goto out_unlock;

	if (!test_bit(WL1271_FLAG_INTENDED_FW_RECOVERY, &wl->flags)) {
		wl12xx_read_fwlog_panic(wl);
		wl12xx_print_recovery(wl);
	}

	BUG_ON(bug_on_recovery &&
	       !test_bit(WL1271_FLAG_INTENDED_FW_RECOVERY, &wl->flags));

	/*
	 * Advance security sequence number to overcome potential progress
	 * in the firmware during recovery. This doens't hurt if the network is
	 * not encrypted.
	 */
	wl12xx_for_each_wlvif(wl, wlvif) {
		if (test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags) ||
		    test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags))
			wlvif->tx_security_seq +=
				WL1271_TX_SQN_POST_RECOVERY_PADDING;
	}

	/* Prevent spurious TX during FW restart */
	ieee80211_stop_queues(wl->hw);

	if (wl->sched_scanning) {
		ieee80211_sched_scan_stopped(wl->hw);
		wl->sched_scanning = false;
	}

	/* reboot the chipset */
	while (!list_empty(&wl->wlvif_list)) {
		wlvif = list_first_entry(&wl->wlvif_list,
				       struct wl12xx_vif, list);
		vif = wl12xx_wlvif_to_vif(wlvif);
		__wl1271_op_remove_interface(wl, vif, false);
	}
	wl1271_op_stop_locked(wl);

	ieee80211_restart_hw(wl->hw);

	/*
	 * Its safe to enable TX now - the queues are stopped after a request
	 * to restart the HW.
	 */
	ieee80211_wake_queues(wl->hw);
out_unlock:
	wl->watchdog_recovery = false;
	if (test_and_clear_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS,
			       &wl->flags))
		wl1271_enable_interrupts(wl);
	mutex_unlock(&wl->mutex);
}

static int wl1271_fw_wakeup(struct wl1271 *wl)
{
	u32 elp_reg;

	elp_reg = ELPCTRL_WAKE_UP;
	return wl1271_raw_write32(wl, HW_ACCESS_ELP_CTRL_REG_ADDR, elp_reg);
}

static int wl1271_setup(struct wl1271 *wl)
{
	wl->fw_status = kmalloc(sizeof(*wl->fw_status), GFP_KERNEL);
	if (!wl->fw_status)
		return -ENOMEM;

	wl->tx_res_if = kmalloc(sizeof(*wl->tx_res_if), GFP_KERNEL);
	if (!wl->tx_res_if) {
		kfree(wl->fw_status);
		return -ENOMEM;
	}

	return 0;
}

static int wl12xx_set_power_on(struct wl1271 *wl)
{
	int ret;

	msleep(WL1271_PRE_POWER_ON_SLEEP);
	ret = wl1271_power_on(wl);
	if (ret < 0)
		goto out;
	msleep(WL1271_POWER_ON_SLEEP);
	wl1271_io_reset(wl);
	wl1271_io_init(wl);

	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_DOWN]);
	if (ret < 0)
		goto fail;

	/* ELP module wake up */
	ret = wl1271_fw_wakeup(wl);
	if (ret < 0)
		goto fail;

out:
	return ret;

fail:
	wl1271_power_off(wl);
	return ret;
}

static int wl12xx_chip_wakeup(struct wl1271 *wl, bool plt)
{
	int ret = 0;

	ret = wl12xx_set_power_on(wl);
	if (ret < 0)
		goto out;

	/*
	 * For wl127x based devices we could use the default block
	 * size (512 bytes), but due to a bug in the sdio driver, we
	 * need to set it explicitly after the chip is powered on.  To
	 * simplify the code and since the performance impact is
	 * negligible, we use the same block size for all different
	 * chip types.
	 */
	if (!wl1271_set_block_size(wl))
		wl->quirks |= WL12XX_QUIRK_NO_BLOCKSIZE_ALIGNMENT;

	switch (wl->chip.id) {
	case CHIP_ID_1271_PG10:
		wl1271_warning("chip id 0x%x (1271 PG10) support is obsolete",
			       wl->chip.id);

		ret = wl1271_setup(wl);
		if (ret < 0)
			goto out;
		wl->quirks |= WL12XX_QUIRK_NO_BLOCKSIZE_ALIGNMENT;
		break;

	case CHIP_ID_1271_PG20:
		wl1271_debug(DEBUG_BOOT, "chip id 0x%x (1271 PG20)",
			     wl->chip.id);

		ret = wl1271_setup(wl);
		if (ret < 0)
			goto out;
		wl->quirks |= WL12XX_QUIRK_NO_BLOCKSIZE_ALIGNMENT;
		break;

	case CHIP_ID_1283_PG20:
		wl1271_debug(DEBUG_BOOT, "chip id 0x%x (1283 PG20)",
			     wl->chip.id);

		ret = wl1271_setup(wl);
		if (ret < 0)
			goto out;
		break;
	case CHIP_ID_1283_PG10:
	default:
		wl1271_warning("unsupported chip id: 0x%x", wl->chip.id);
		ret = -ENODEV;
		goto out;
	}

	ret = wl12xx_fetch_firmware(wl, plt);
	if (ret < 0)
		goto out;

	/* No NVS from netlink, try to get it from the filesystem */
	if (wl->nvs == NULL) {
		ret = wl1271_fetch_nvs(wl);
		if (ret < 0)
			goto out;
	}

out:
	return ret;
}

int wl1271_plt_start(struct wl1271 *wl)
{
	int retries = WL1271_BOOT_RETRIES;
	struct wiphy *wiphy = wl->hw->wiphy;
	int ret;

	mutex_lock(&wl->mutex);

	wl1271_notice("power up");

	if (wl->state != WL1271_STATE_OFF) {
		wl1271_error("cannot go into PLT state because not "
			     "in off state: %d", wl->state);
		ret = -EBUSY;
		goto out;
	}

	while (retries) {
		retries--;
		ret = wl12xx_chip_wakeup(wl, true);
		if (ret < 0)
			goto power_off;

		ret = wl1271_boot(wl);
		if (ret < 0)
			goto power_off;

		ret = wl1271_plt_init(wl);
		if (ret < 0)
			goto irq_disable;

		wl->state = WL1271_STATE_PLT;
		wl1271_notice("firmware booted in PLT mode (%s)",
			      wl->chip.fw_ver_str);

		/* update hw/fw version info in wiphy struct */
		wiphy->hw_version = wl->chip.id;
		strncpy(wiphy->fw_version, wl->chip.fw_ver_str,
			sizeof(wiphy->fw_version));

		goto out;

irq_disable:
		mutex_unlock(&wl->mutex);
		/* Unlocking the mutex in the middle of handling is
		   inherently unsafe. In this case we deem it safe to do,
		   because we need to let any possibly pending IRQ out of
		   the system (and while we are WL1271_STATE_OFF the IRQ
		   work function will not do anything.) Also, any other
		   possible concurrent operations will fail due to the
		   current state, hence the wl1271 struct should be safe. */
		wl1271_disable_interrupts(wl);
		wl1271_flush_deferred_work(wl);
		cancel_work_sync(&wl->netstack_work);
		mutex_lock(&wl->mutex);
power_off:
		wl1271_power_off(wl);
	}

	wl1271_error("firmware boot in PLT mode failed despite %d retries",
		     WL1271_BOOT_RETRIES);
out:
	mutex_unlock(&wl->mutex);

	return ret;
}

int wl1271_plt_stop(struct wl1271 *wl)
{
	int ret = 0;

	wl1271_notice("power down");

	/*
	 * Interrupts must be disabled before setting the state to OFF.
	 * Otherwise, the interrupt handler might be called and exit without
	 * reading the interrupt status.
	 */
	wl1271_disable_interrupts(wl);
	mutex_lock(&wl->mutex);
	if (wl->state != WL1271_STATE_PLT) {
		mutex_unlock(&wl->mutex);

		/*
		 * This will not necessarily enable interrupts as interrupts
		 * may have been disabled when op_stop was called. It will,
		 * however, balance the above call to disable_interrupts().
		 */
		wl1271_enable_interrupts(wl);

		wl1271_error("cannot power down because not in PLT "
			     "state: %d", wl->state);
		ret = -EBUSY;
		goto out;
	}

	mutex_unlock(&wl->mutex);

	wl1271_flush_deferred_work(wl);
	cancel_work_sync(&wl->netstack_work);
	cancel_work_sync(&wl->recovery_work);
	cancel_delayed_work_sync(&wl->elp_work);

	mutex_lock(&wl->mutex);
	wl1271_power_off(wl);
	wl->flags = 0;
	wl->state = WL1271_STATE_OFF;
	wl->rx_counter = 0;
	mutex_unlock(&wl->mutex);

out:
	return ret;
}

static void wl1271_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct wl1271 *wl = hw->priv;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct wl12xx_vif *wlvif = NULL;
	unsigned long flags;
	int q, mapping;
	u8 hlid;

	if (vif)
		wlvif = wl12xx_vif_to_data(vif);

	mapping = skb_get_queue_mapping(skb);
	q = wl1271_tx_get_queue(mapping);

	hlid = wl12xx_tx_get_hlid(wl, wlvif, skb);

	spin_lock_irqsave(&wl->wl_lock, flags);

	/* queue the packet */
	if (hlid == WL12XX_INVALID_LINK_ID ||
	    (wlvif && !test_bit(hlid, wlvif->links_map))) {
		wl1271_debug(DEBUG_TX, "DROP skb hlid %d q %d", hlid, q);
		ieee80211_free_txskb(hw, skb);
		goto out;
	}

	wl1271_debug(DEBUG_TX, "queue skb hlid %d q %d", hlid, q);
	skb_queue_tail(&wl->links[hlid].tx_queue[q], skb);

	wl->tx_queue_count[q]++;

	/*
	 * The workqueue is slow to process the tx_queue and we need stop
	 * the queue here, otherwise the queue will get too long.
	 */
	if (wl->tx_queue_count[q] >= WL1271_TX_QUEUE_HIGH_WATERMARK) {
		wl1271_debug(DEBUG_TX, "op_tx: stopping queues for q %d", q);
		ieee80211_stop_queue(wl->hw, mapping);
		set_bit(q, &wl->stopped_queues_map);
	}

	/*
	 * The chip specific setup must run before the first TX packet -
	 * before that, the tx_work will not be initialized!
	 */

	if (!test_bit(WL1271_FLAG_FW_TX_BUSY, &wl->flags) &&
	    !test_bit(WL1271_FLAG_TX_PENDING, &wl->flags))
		ieee80211_queue_work(wl->hw, &wl->tx_work);

out:
	spin_unlock_irqrestore(&wl->wl_lock, flags);
}

int wl1271_tx_dummy_packet(struct wl1271 *wl)
{
	unsigned long flags;
	int q;
	int ret = 0;

	/* no need to queue a new dummy packet if one is already pending */
	if (test_bit(WL1271_FLAG_DUMMY_PACKET_PENDING, &wl->flags))
		return 0;

	q = wl1271_tx_get_queue(skb_get_queue_mapping(wl->dummy_packet));

	spin_lock_irqsave(&wl->wl_lock, flags);
	set_bit(WL1271_FLAG_DUMMY_PACKET_PENDING, &wl->flags);
	wl->tx_queue_count[q]++;
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	/* The FW is low on RX memory blocks, so send the dummy packet asap */
	if (!test_bit(WL1271_FLAG_FW_TX_BUSY, &wl->flags))
		ret = wl1271_tx_work_locked(wl);

	/*
	 * If the FW TX is busy, TX work will be scheduled by the threaded
	 * interrupt handler function
	 */
	return ret;
}

/*
 * The size of the dummy packet should be at least 1400 bytes. However, in
 * order to minimize the number of bus transactions, aligning it to 512 bytes
 * boundaries could be beneficial, performance wise
 */
#define TOTAL_TX_DUMMY_PACKET_SIZE (ALIGN(1400, 512))

static struct sk_buff *wl12xx_alloc_dummy_packet(struct wl1271 *wl)
{
	struct sk_buff *skb;
	struct ieee80211_hdr_3addr *hdr;
	unsigned int dummy_packet_size;

	dummy_packet_size = TOTAL_TX_DUMMY_PACKET_SIZE -
			    sizeof(struct wl1271_tx_hw_descr) - sizeof(*hdr);

	skb = dev_alloc_skb(TOTAL_TX_DUMMY_PACKET_SIZE);
	if (!skb) {
		wl1271_warning("Failed to allocate a dummy packet skb");
		return NULL;
	}

	skb_reserve(skb, sizeof(struct wl1271_tx_hw_descr));

	hdr = (struct ieee80211_hdr_3addr *) skb_put(skb, sizeof(*hdr));
	memset(hdr, 0, sizeof(*hdr));
	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA |
					 IEEE80211_STYPE_NULLFUNC |
					 IEEE80211_FCTL_TODS);

	memset(skb_put(skb, dummy_packet_size), 0, dummy_packet_size);

	/* Dummy packets require the TID to be management */
	skb->priority = WL1271_TID_MGMT;

	/* Initialize all fields that might be used */
	skb_set_queue_mapping(skb, 0);
	memset(IEEE80211_SKB_CB(skb), 0, sizeof(struct ieee80211_tx_info));

	return skb;
}


static struct notifier_block wl1271_dev_notifier = {
	.notifier_call = wl1271_dev_notify,
};

#ifdef CONFIG_PM
int wl1271_validate_wowlan_pattern(struct cfg80211_wowlan_trig_pkt_pattern *p)
{
	int num_fields = 0, in_field = 0, fields_size = 0;
	int i, pattern_len = 0;

	if (!p->mask) {
		wl1271_warning("No mask in WoWLAN pattern");
		return -EINVAL;
	}

	/* The pattern is broken up into segments of bytes at different offsets
	 * that need to be checked by the FW filter. Each segment is called
	 * a field in the FW API. We verify that the total number of fields
	 * required for this pattern won't exceed FW limits (8)
	 * as well as the total fields  buffer won't exceed the FW limit.
	 * Note that if there's a pattern which crosses Ethernet/IP header
	 * boundary a new field is required.
	 */
	for (i = 0; i < p->pattern_len; i++) {
		if (test_bit(i, (unsigned long *)p->mask)) {
			if (!in_field) {
				in_field = 1;
				pattern_len = 1;
			} else {
				if (i == WL1271_RX_FILTER_ETH_HEADER_SIZE) {
					num_fields++;
					fields_size += pattern_len +
						RX_FILTER_FIELD_OVERHEAD;
					pattern_len = 1;
				} else
					pattern_len++;
			}
		} else {
			if (in_field) {
				in_field = 0;
				fields_size += pattern_len +
					RX_FILTER_FIELD_OVERHEAD;
				num_fields++;
			}
		}
	}

	if (in_field) {
		fields_size += pattern_len + RX_FILTER_FIELD_OVERHEAD;
		num_fields++;
	}

	if (num_fields > WL1271_RX_FILTER_MAX_FIELDS) {
		wl1271_warning("RX Filter too complex. Too many segments");
		return -EINVAL;
	}

	if (fields_size > WL1271_RX_FILTER_MAX_FIELDS_SIZE) {
		wl1271_warning("RX filter pattern is too big");
		return -E2BIG;
	}

	return 0;
}

struct wl12xx_rx_data_filter *wl1271_rx_filter_alloc(void)
{
	return kzalloc(sizeof(struct wl12xx_rx_data_filter), GFP_KERNEL);
}

void wl1271_rx_filter_free(struct wl12xx_rx_data_filter *filter)
{
	int i;

	if (filter == NULL)
		return;

	for (i = 0; i < filter->num_fields; i++)
		kfree(filter->fields[i].pattern);

	kfree(filter);
}

int wl1271_rx_filter_alloc_field(struct wl12xx_rx_data_filter *filter,
				 u16 offset, u8 flags,
				 u8 *pattern, u8 len)
{
	struct wl12xx_rx_data_filter_field *field;

	if (filter->num_fields == WL1271_RX_FILTER_MAX_FIELDS) {
		wl1271_warning("Max fields per RX filter. can't alloc another");
		return -EINVAL;
	}

	field = &filter->fields[filter->num_fields];

	field->pattern = kzalloc(len, GFP_KERNEL);
	if (!field->pattern) {
		wl1271_warning("Failed to allocate RX filter pattern");
		return -ENOMEM;
	}

	filter->num_fields++;

	field->offset = cpu_to_le16(offset);
	field->flags = flags;
	field->len = len;
	memcpy(field->pattern, pattern, len);

	return 0;
}

int wl1271_rx_filter_get_fields_size(struct wl12xx_rx_data_filter *filter)
{
	int i, fields_size = 0;

	for (i = 0; i < filter->num_fields; i++)
		fields_size += filter->fields[i].len +
			sizeof(struct wl12xx_rx_data_filter_field) -
			sizeof(u8 *);

	return fields_size;
}

void wl1271_rx_filter_flatten_fields(struct wl12xx_rx_data_filter *filter,
				    u8 *buf)
{
	int i;
	struct wl12xx_rx_data_filter_field *field;

	for (i = 0; i < filter->num_fields; i++) {
		field = (struct wl12xx_rx_data_filter_field *)buf;

		field->offset = filter->fields[i].offset;
		field->flags = filter->fields[i].flags;
		field->len = filter->fields[i].len;

		memcpy(&field->pattern, filter->fields[i].pattern, field->len);
		buf += sizeof(struct wl12xx_rx_data_filter_field) -
			sizeof(u8 *) + field->len;
	}
}

/* Allocates an RX filter returned through f
 * which needs to be freed using rx_filter_free()
 */
int wl1271_convert_wowlan_pattern_to_rx_filter(
	struct cfg80211_wowlan_trig_pkt_pattern *p,
	struct wl12xx_rx_data_filter **f)
{
	int i, j, ret = 0;
	struct wl12xx_rx_data_filter *filter;
	u16 offset;
	u8 flags, len;

	filter = wl1271_rx_filter_alloc();
	if (!filter) {
		wl1271_warning("Failed to alloc rx filter");
		ret = -ENOMEM;
		goto err;
	}

	i = 0;
	while (i < p->pattern_len) {
		if (!test_bit(i, (unsigned long *)p->mask)) {
			i++;
			continue;
		}

		for (j = i; j < p->pattern_len; j++) {
			if (!test_bit(j, (unsigned long *)p->mask))
				break;

			if (i < WL1271_RX_FILTER_ETH_HEADER_SIZE &&
			    j >= WL1271_RX_FILTER_ETH_HEADER_SIZE)
				break;
		}

		if (i < WL1271_RX_FILTER_ETH_HEADER_SIZE) {
			offset = i;
			flags = WL1271_RX_FILTER_FLAG_ETHERNET_HEADER;
		} else {
			offset = i - WL1271_RX_FILTER_ETH_HEADER_SIZE;
			flags = WL1271_RX_FILTER_FLAG_IP_HEADER;
		}

		len = j - i;

		ret = wl1271_rx_filter_alloc_field(filter,
						   offset,
						   flags,
						   &p->pattern[i], len);
		if (ret)
			goto err;

		i = j;
	}

	switch (p->action) {
	case NL80211_WOWLAN_ACTION_ALLOW:
		filter->action = FILTER_SIGNAL;
		break;
	case NL80211_WOWLAN_ACTION_DROP:
		filter->action = FILTER_DROP;
		break;
	default:
		wl1271_warning("Bad wowlan action: %d", p->action);
		ret = -EINVAL;
		goto err;
	}

	*f = filter;
	return 0;

err:
	wl1271_rx_filter_free(filter);
	*f = NULL;

	return ret;
}

static int wl1271_configure_wowlan(struct wl1271 *wl,
				   struct cfg80211_wowlan *wow)
{
	int i, ret;

	if (!wow || wow->any || !wow->n_patterns) {
		ret = wl1271_rx_data_filtering_enable(wl, 0, FILTER_SIGNAL);
		if (ret < 0)
			goto out;

		ret = wl1271_rx_data_filters_clear_all(wl);
		if (ret < 0)
			goto out;
		return 0;
	}

	WARN_ON(wow->n_patterns > WL1271_MAX_RX_FILTERS);

	/* Validate all incoming patterns before clearing current FW state */
	for (i = 0; i < wow->n_patterns; i++) {
		ret = wl1271_validate_wowlan_pattern(&wow->patterns[i]);
		if (ret) {
			wl1271_warning("validate_wowlan_pattern "
				       "failed (%d)", ret);
			return ret;
		}
	}

	ret = wl1271_rx_data_filtering_enable(wl, 0, FILTER_SIGNAL);
	if (ret < 0)
		goto out;

	ret = wl1271_rx_data_filters_clear_all(wl);
	if (ret < 0)
		goto out;

	/* Translate WoWLAN patterns into filters */
	for (i = 0; i < wow->n_patterns; i++) {
		struct cfg80211_wowlan_trig_pkt_pattern *p;
		struct wl12xx_rx_data_filter *filter = NULL;

		p = &wow->patterns[i];

		ret = wl1271_convert_wowlan_pattern_to_rx_filter(p, &filter);
		if (ret) {
			wl1271_warning("convert_wowlan_pattern_to_rx_filter "
				       "failed (%d)", ret);
			goto out;
		}

		ret = wl1271_rx_data_filter_enable(wl, i, 1, filter);

		wl1271_rx_filter_free(filter);
		if (ret) {
			wl1271_warning("rx_data_filter_enable "
				       " failed (%d)", ret);
			goto out;
		}
	}

	ret = wl1271_rx_data_filtering_enable(wl, 1, FILTER_DROP);
	if (ret) {
		wl1271_warning("rx_data_filtering_enable failed (%d)", ret);
		goto out;
	}

out:
	return ret;
}

static int wl1271_configure_suspend_sta(struct wl1271 *wl,
					struct wl12xx_vif *wlvif,
					struct cfg80211_wowlan *wow)
{
	int ret = 0;

	if (!test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_acx_wake_up_conditions(wl, wlvif,
				    wl->conf.conn.suspend_wake_up_event,
				    wl->conf.conn.suspend_listen_interval);

	if (ret < 0)
		wl1271_error("suspend: set wake up conditions failed: %d", ret);

	wl1271_ps_elp_sleep(wl);

out:
	return ret;

}

static int wl1271_configure_suspend_ap(struct wl1271 *wl,
				       struct wl12xx_vif *wlvif)
{
	int ret = 0;

	if (!test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_acx_beacon_filter_opt(wl, wlvif, true);

	wl1271_ps_elp_sleep(wl);
out:
	return ret;

}

static int wl1271_configure_suspend(struct wl1271 *wl,
				    struct wl12xx_vif *wlvif,
					struct cfg80211_wowlan *wow)
{
	if (wlvif->bss_type == BSS_TYPE_STA_BSS)
		return wl1271_configure_suspend_sta(wl, wlvif, wow);
	if (wlvif->bss_type == BSS_TYPE_AP_BSS)
		return wl1271_configure_suspend_ap(wl, wlvif);
	return 0;
}

static void wl1271_configure_resume(struct wl1271 *wl,
				    struct wl12xx_vif *wlvif)
{
	int ret = 0;
	bool is_ap = wlvif->bss_type == BSS_TYPE_AP_BSS;
	bool is_sta = wlvif->bss_type == BSS_TYPE_STA_BSS;

	if ((!is_ap) && (!is_sta))
		return;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		return;

	if (is_sta) {
		ret = wl1271_acx_wake_up_conditions(wl, wlvif,
				    wl->conf.conn.wake_up_event,
				    wl->conf.conn.listen_interval);

		if (ret < 0)
			wl1271_error("resume: wake up conditions failed: %d",
				     ret);

	} else if (is_ap) {
		ret = wl1271_acx_beacon_filter_opt(wl, wlvif, false);
	}

	wl1271_ps_elp_sleep(wl);
}

static int wl1271_op_suspend(struct ieee80211_hw *hw,
			    struct cfg80211_wowlan *wow)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif;
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 suspend wow=%d", !!wow);
	WARN_ON(!wow);

	/* we want to perform the recovery before suspending */
	if (test_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS, &wl->flags)) {
		wl1271_warning("postponing suspend to perform recovery");
#ifdef CONFIG_HAS_WAKELOCK
		/* give us a grace period for recovery */
		wake_lock_timeout(&wl->recovery_wake, 5 * HZ);
#endif
		return -EBUSY;
	}

	wl1271_tx_flush(wl);

	mutex_lock(&wl->mutex);
	wl->wow_enabled = true;
	wl12xx_for_each_wlvif(wl, wlvif) {
		ret = wl1271_configure_suspend(wl, wlvif, wow);
		if (ret < 0) {
			mutex_unlock(&wl->mutex);
			wl1271_warning("couldn't prepare device to suspend");
			return ret;
		}
	}
	mutex_unlock(&wl->mutex);
	/* flush any remaining work */
	wl1271_debug(DEBUG_MAC80211, "flushing remaining works");

	/*
	 * disable and re-enable interrupts in order to flush
	 * the threaded_irq
	 */
	wl1271_disable_interrupts(wl);

	/*
	 * set suspended flag to avoid triggering a new threaded_irq
	 * work. no need for spinlock as interrupts are disabled.
	 */
	set_bit(WL1271_FLAG_SUSPENDED, &wl->flags);

	wl1271_enable_interrupts(wl);
	flush_work(&wl->tx_work);
	flush_delayed_work(&wl->elp_work);

	return 0;
}

static int wl1271_op_resume(struct ieee80211_hw *hw)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif;
	unsigned long flags;
	bool run_irq_work = false, pending_recovery;
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 resume wow=%d",
		     wl->wow_enabled);
	WARN_ON(!wl->wow_enabled);

	/*
	 * re-enable irq_work enqueuing, and call irq_work directly if
	 * there is a pending work.
	 */
	spin_lock_irqsave(&wl->wl_lock, flags);
	clear_bit(WL1271_FLAG_SUSPENDED, &wl->flags);
	if (test_and_clear_bit(WL1271_FLAG_PENDING_WORK, &wl->flags))
		run_irq_work = true;
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	mutex_lock(&wl->mutex);

	/* test the recovery flag before calling any SDIO functions */
	pending_recovery = test_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS,
				    &wl->flags);

	if (run_irq_work) {
		wl1271_debug(DEBUG_MAC80211,
			     "run postponed irq_work directly");

		/* don't talk to the HW if recovery is pending */
		if (!pending_recovery) {
			ret = wl12xx_irq_locked(wl);
			if (ret)
				wl12xx_queue_recovery_work(wl);
		}

		wl1271_enable_interrupts(wl);
	}

	if (pending_recovery) {
		wl1271_warning("queuing forgotten recovery on resume");
		ieee80211_queue_work(wl->hw, &wl->recovery_work);
		goto out;
	}

	wl12xx_for_each_wlvif(wl, wlvif) {
		wl1271_configure_resume(wl, wlvif);
	}

out:
	wl->wow_enabled = false;
	mutex_unlock(&wl->mutex);

	return 0;
}
#endif /* CONFIG_PM */

static int wl1271_op_start(struct ieee80211_hw *hw)
{
	struct wl1271 *wl = hw->priv;

	wl1271_debug(DEBUG_MAC80211, "mac80211 start");

	/*
	 * We have to delay the booting of the hardware because
	 * we need to know the local MAC address before downloading and
	 * initializing the firmware. The MAC address cannot be changed
	 * after boot, and without the proper MAC address, the firmware
	 * will not function properly.
	 *
	 * The MAC address is first known when the corresponding interface
	 * is added. That is where we will initialize the hardware.
	 */


	/*
	 * store wl in the global wl_list, used to find wl
	 * in the wl1271_dev_notify callback
	 */
	mutex_lock(&wl_list_mutex);
	list_add(&wl->list, &wl_list);
	mutex_unlock(&wl_list_mutex);
	return 0;
}

static void wl1271_op_stop_locked(struct wl1271 *wl)
{
	int i;

	if (wl->state == WL1271_STATE_OFF) {
		if (test_and_clear_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS,
				       &wl->flags))
			wl1271_enable_interrupts(wl);

		return;
	}

	/*
	 * this must be before the cancel_work calls below, so that the work
	 * functions don't perform further work.
	 */
	wl->state = WL1271_STATE_OFF;

	/*
	 * Use the nosync variant to disable interrupts, so the mutex could be
	 * held while doing so without deadlocking.
	 */
	wlcore_disable_interrupts_nosync(wl);
	mutex_unlock(&wl->mutex);

	mutex_lock(&wl_list_mutex);
	list_del(&wl->list);
	mutex_unlock(&wl_list_mutex);

	wlcore_synchronize_interrupts(wl);
	wl1271_flush_deferred_work(wl);
	cancel_delayed_work_sync(&wl->scan_complete_work);
	cancel_work_sync(&wl->netstack_work);
	cancel_work_sync(&wl->tx_work);
	cancel_delayed_work_sync(&wl->elp_work);

	/* let's notify MAC80211 about the remaining pending TX frames */
	wl12xx_tx_reset(wl, true);
	mutex_lock(&wl->mutex);

	wl1271_power_off(wl);
	/*
	 * In case a recovery was scheduled, interrupts were disabled to avoid
	 * an interrupt storm. Now that the power is down, it is safe to
	 * re-enable interrupts to balance the disable depth
	 */
	if (test_and_clear_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS, &wl->flags))
		wl1271_enable_interrupts(wl);

	wl->fw_type = WL12XX_FW_TYPE_NONE;

	wl->band = IEEE80211_BAND_2GHZ;

	wl->rx_counter = 0;
	wl->power_level = WL1271_DEFAULT_POWER_LEVEL;
	wl->tx_blocks_available = 0;
	wl->tx_allocated_blocks = 0;
	wl->tx_results_count = 0;
	wl->tx_packets_count = 0;
	wl->time_offset = 0;
	wl->ap_fw_ps_map = 0;
	wl->ap_ps_map = 0;
	wl->sched_scanning = false;
	memset(wl->roles_map, 0, sizeof(wl->roles_map));
	memset(wl->links_map, 0, sizeof(wl->links_map));
	memset(wl->roc_map, 0, sizeof(wl->roc_map));
	wl->active_sta_count = 0;

	/* The system link is always allocated */
	__set_bit(WL12XX_SYSTEM_HLID, wl->links_map);

	/*
	 * this is performed after the cancel_work calls and the associated
	 * mutex_lock, so that wl1271_op_add_interface does not accidentally
	 * get executed before all these vars have been reset.
	 */
	wl->flags = 0;

	wl->tx_blocks_freed = 0;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		wl->tx_pkts_freed[i] = 0;
		wl->tx_allocated_pkts[i] = 0;
	}

	wl1271_debugfs_reset(wl);

	kfree(wl->fw_status);
	wl->fw_status = NULL;
	kfree(wl->tx_res_if);
	wl->tx_res_if = NULL;
	kfree(wl->target_mem_map);
	wl->target_mem_map = NULL;
}

static void wl1271_op_stop(struct ieee80211_hw *hw)
{
	struct wl1271 *wl = hw->priv;

	wl1271_debug(DEBUG_MAC80211, "mac80211 stop");

	mutex_lock(&wl->mutex);

	wl1271_op_stop_locked(wl);

	mutex_unlock(&wl->mutex);
}

static int wl12xx_allocate_rate_policy(struct wl1271 *wl, u8 *idx)
{
	u8 policy = find_first_zero_bit(wl->rate_policies_map,
					WL12XX_MAX_RATE_POLICIES);
	if (policy >= WL12XX_MAX_RATE_POLICIES)
		return -EBUSY;

	__set_bit(policy, wl->rate_policies_map);
	*idx = policy;
	return 0;
}

static void wl12xx_free_rate_policy(struct wl1271 *wl, u8 *idx)
{
	if (WARN_ON(*idx >= WL12XX_MAX_RATE_POLICIES))
		return;

	__clear_bit(*idx, wl->rate_policies_map);
	*idx = WL12XX_MAX_RATE_POLICIES;
}

static u8 wl12xx_get_role_type(struct wl1271 *wl, struct wl12xx_vif *wlvif)
{
	switch (wlvif->bss_type) {
	case BSS_TYPE_AP_BSS:
		if (wlvif->p2p)
			return WL1271_ROLE_P2P_GO;
		else
			return WL1271_ROLE_AP;

	case BSS_TYPE_STA_BSS:
		if (wlvif->p2p)
			return WL1271_ROLE_P2P_CL;
		else
			return WL1271_ROLE_STA;

	case BSS_TYPE_IBSS:
		return WL1271_ROLE_IBSS;

	default:
		wl1271_error("invalid bss_type: %d", wlvif->bss_type);
	}
	return WL12XX_INVALID_ROLE_TYPE;
}

static int wl12xx_init_vif_data(struct wl1271 *wl, struct ieee80211_vif *vif)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int i;

	/* clear everything but the persistent data */
	memset(wlvif, 0, offsetof(struct wl12xx_vif, persistent));

	switch (ieee80211_vif_type_p2p(vif)) {
	case NL80211_IFTYPE_P2P_CLIENT:
		wlvif->p2p = 1;
		/* fall-through */
	case NL80211_IFTYPE_STATION:
		wlvif->bss_type = BSS_TYPE_STA_BSS;
		break;
	case NL80211_IFTYPE_ADHOC:
		wlvif->bss_type = BSS_TYPE_IBSS;
		break;
	case NL80211_IFTYPE_P2P_GO:
		wlvif->p2p = 1;
		/* fall-through */
	case NL80211_IFTYPE_AP:
		wlvif->bss_type = BSS_TYPE_AP_BSS;
		break;
	default:
		wlvif->bss_type = MAX_BSS_TYPE;
		return -EOPNOTSUPP;
	}

	wlvif->role_id = WL12XX_INVALID_ROLE_ID;
	wlvif->dev_role_id = WL12XX_INVALID_ROLE_ID;
	wlvif->dev_hlid = WL12XX_INVALID_LINK_ID;

	if (wlvif->bss_type == BSS_TYPE_STA_BSS ||
	    wlvif->bss_type == BSS_TYPE_IBSS) {
		/* init sta/ibss data */
		wlvif->sta.hlid = WL12XX_INVALID_LINK_ID;
		wl12xx_allocate_rate_policy(wl, &wlvif->sta.basic_rate_idx);
		wl12xx_allocate_rate_policy(wl, &wlvif->sta.ap_rate_idx);
		wl12xx_allocate_rate_policy(wl, &wlvif->sta.p2p_rate_idx);
	} else {
		/* init ap data */
		wlvif->ap.bcast_hlid = WL12XX_INVALID_LINK_ID;
		wlvif->ap.global_hlid = WL12XX_INVALID_LINK_ID;
		wl12xx_allocate_rate_policy(wl, &wlvif->ap.mgmt_rate_idx);
		wl12xx_allocate_rate_policy(wl, &wlvif->ap.bcast_rate_idx);
		for (i = 0; i < CONF_TX_MAX_AC_COUNT; i++)
			wl12xx_allocate_rate_policy(wl,
						&wlvif->ap.ucast_rate_idx[i]);
	}

	wlvif->bitrate_masks[IEEE80211_BAND_2GHZ] = wl->conf.tx.basic_rate;
	wlvif->bitrate_masks[IEEE80211_BAND_5GHZ] = wl->conf.tx.basic_rate_5;
	wlvif->basic_rate_set = CONF_TX_RATE_MASK_BASIC;
	wlvif->basic_rate = CONF_TX_RATE_MASK_BASIC;
	wlvif->rate_set = CONF_TX_RATE_MASK_BASIC;
	wlvif->beacon_int = WL1271_DEFAULT_BEACON_INT;

	/*
	 * mac80211 configures some values globally, while we treat them
	 * per-interface. thus, on init, we have to copy them from wl
	 */
	wlvif->band = wl->band;
	wlvif->channel = wl->channel;
	wlvif->power_level = wl->power_level;

	INIT_WORK(&wlvif->rx_streaming_enable_work,
		  wl1271_rx_streaming_enable_work);
	INIT_WORK(&wlvif->rx_streaming_disable_work,
		  wl1271_rx_streaming_disable_work);
	INIT_LIST_HEAD(&wlvif->list);

	setup_timer(&wlvif->rx_streaming_timer, wl1271_rx_streaming_timer,
		    (unsigned long) wlvif);
	return 0;
}

static bool wl12xx_init_fw(struct wl1271 *wl)
{
	int retries = WL1271_BOOT_RETRIES;
	bool booted = false;
	struct wiphy *wiphy = wl->hw->wiphy;
	int ret;

	while (retries) {
		retries--;
		ret = wl12xx_chip_wakeup(wl, false);
		if (ret < 0)
			goto power_off;

		ret = wl1271_boot(wl);
		if (ret < 0)
			goto power_off;

		ret = wl1271_hw_init(wl);
		if (ret < 0)
			goto irq_disable;

		booted = true;
		break;

irq_disable:
		mutex_unlock(&wl->mutex);
		/* Unlocking the mutex in the middle of handling is
		   inherently unsafe. In this case we deem it safe to do,
		   because we need to let any possibly pending IRQ out of
		   the system (and while we are WL1271_STATE_OFF the IRQ
		   work function will not do anything.) Also, any other
		   possible concurrent operations will fail due to the
		   current state, hence the wl1271 struct should be safe. */
		wl1271_disable_interrupts(wl);
		wl1271_flush_deferred_work(wl);
		cancel_work_sync(&wl->netstack_work);
		mutex_lock(&wl->mutex);
power_off:
		wl1271_power_off(wl);
	}

	if (!booted) {
		wl1271_error("firmware boot failed despite %d retries",
			     WL1271_BOOT_RETRIES);
		goto out;
	}

	wl1271_info("firmware booted (%s)", wl->chip.fw_ver_str);

	/* update hw/fw version info in wiphy struct */
	wiphy->hw_version = wl->chip.id;
	strncpy(wiphy->fw_version, wl->chip.fw_ver_str,
		sizeof(wiphy->fw_version));

	/*
	 * Now we know if 11a is supported (info from the NVS), so disable
	 * 11a channels if not supported
	 */
	if (!wl->enable_11a)
		wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels = 0;

	wl1271_debug(DEBUG_MAC80211, "11a is %ssupported",
		     wl->enable_11a ? "" : "not ");

	wl->state = WL1271_STATE_ON;
	wl->watchdog_recovery = false;
out:
	return booted;
}

static bool wl12xx_dev_role_started(struct wl12xx_vif *wlvif)
{
	return wlvif->dev_hlid != WL12XX_INVALID_LINK_ID;
}

static bool wl12xx_need_fw_change(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  enum wl12xx_fw_type current_fw,
				  bool add)
{
	struct wl1271 *wl = hw->priv;
	u8 open_count;

	if (ieee80211_suspending(hw))
		return false;

	if (test_bit(WL1271_FLAG_VIF_CHANGE_IN_PROGRESS, &wl->flags))
		return false;

	open_count = ieee80211_get_open_count(hw, vif);
	wl1271_info("open_count=%d, add=%d, current_fw=%d",
		open_count, add, current_fw);
	if (add)
		open_count++;

	if (open_count > 1 && current_fw == WL12XX_FW_TYPE_NORMAL)
		return true;
	if (open_count <= 1 && current_fw == WL12XX_FW_TYPE_MULTI)
		return true;

	return false;

}

/*
 * enter "forced psm". this is an ugly optimization used to make
 * fw change a bit more disconnection-persistent.
 */
static void wl12xx_force_active_psm(struct wl1271 *wl)
{
	struct wl12xx_vif *wlvif;

	wl12xx_for_each_wlvif_sta(wl, wlvif) {
		wl1271_ps_set_mode(wl, wlvif, STATION_POWER_SAVE_MODE);
	}
}
static int wl1271_op_add_interface(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret = 0;
	u8 role_type;
	int open_count;
	bool booted = false;

	wl1271_debug(DEBUG_MAC80211, "mac80211 add interface type %d mac %pM",
		     ieee80211_vif_type_p2p(vif), vif->addr);

	mutex_lock(&wl->mutex);
	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out_unlock;

	/*
	 * in some very corner case HW recovery scenarios its possible to
	 * get here before __wl1271_op_remove_interface is complete, so
	 * opt out if that is the case.
	 */
	if (test_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS, &wl->flags) ||
	    test_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags)) {
		ret = -EBUSY;
		goto out;
	}


	ret = wl12xx_init_vif_data(wl, vif);
	if (ret < 0)
		goto out;

	wlvif->wl = wl;
	role_type = wl12xx_get_role_type(wl, wlvif);
	if (role_type == WL12XX_INVALID_ROLE_TYPE) {
		ret = -EINVAL;
		goto out;
	}

	if (wl12xx_need_fw_change(hw, vif, wl->fw_type, true)) {
		wl12xx_force_active_psm(wl);
		set_bit(WL1271_FLAG_INTENDED_FW_RECOVERY, &wl->flags);
		mutex_unlock(&wl->mutex);
		wl1271_recovery_work(&wl->recovery_work);
		return 0;
	}

	/*
	 * TODO: after the nvs issue will be solved, move this block
	 * to start(), and make sure here the driver is ON.
	 */
	wl1271_info("state: %d", wl->state);
	if (wl->state == WL1271_STATE_OFF) {
		/*
		 * we still need this in order to configure the fw
		 * while uploading the nvs
		 */
		memcpy(wl->addresses[0].addr, vif->addr, ETH_ALEN);

		booted = wl12xx_init_fw(wl);
		if (!booted) {
			ret = -EINVAL;
			goto out;
		}
	}

	if (wlvif->bss_type == BSS_TYPE_STA_BSS ||
	    wlvif->bss_type == BSS_TYPE_IBSS) {
		/*
		 * The device role is a special role used for
		 * rx and tx frames prior to association (as
		 * the STA role can get packets only from
		 * its associated bssid)
		 */
		ret = wl12xx_cmd_role_enable(wl, vif->addr,
						 WL1271_ROLE_DEVICE,
						 &wlvif->dev_role_id);
		if (ret < 0)
			goto out;
	}

	ret = wl12xx_cmd_role_enable(wl, vif->addr,
				     role_type, &wlvif->role_id);
	if (ret < 0)
		goto out;

	ret = wl1271_init_vif_specific(wl, vif);
	if (ret < 0)
		goto out;

	list_add(&wlvif->list, &wl->wlvif_list);
	set_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags);

	if (wlvif->bss_type == BSS_TYPE_AP_BSS)
		wl->ap_count++;
	else
		wl->sta_count++;

	open_count = ieee80211_get_open_count(hw, vif);
	if (open_count > 0)  /* Multi Role */
		ieee80211_roaming_status(vif, false);
	else                 /* Single Role */
		ieee80211_roaming_status(vif, true);

out:
	wl1271_ps_elp_sleep(wl);
out_unlock:
	mutex_unlock(&wl->mutex);

	return ret;
}

static void __wl1271_op_remove_interface(struct wl1271 *wl,
					 struct ieee80211_vif *vif,
					 bool reset_tx_queues)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int i, ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 remove interface");

	if (!test_and_clear_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags))
		return;

	/* because of hardware recovery, we may get here twice */
	if (wl->state != WL1271_STATE_ON)
		return;

	wl1271_info("down");

	if (wl->scan.state != WL1271_SCAN_STATE_IDLE &&
	    wl->scan_vif == vif) {
		wl->scan.state = WL1271_SCAN_STATE_IDLE;
		memset(wl->scan.scanned_ch, 0, sizeof(wl->scan.scanned_ch));
		wl->scan_vif = NULL;
		wl->scan.req = NULL;
		ieee80211_scan_completed(wl->hw, true);
	}

	if (!test_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS, &wl->flags)) {
		/* disable active roles and clear RX filters */
		ret = wl1271_ps_elp_wakeup(wl);
		if (ret < 0)
			goto deinit;

		if (wlvif->bss_type == BSS_TYPE_STA_BSS ||
		    wlvif->bss_type == BSS_TYPE_IBSS) {
			ret = wl1271_configure_wowlan(wl, NULL);
			if (ret < 0)
				goto deinit;

			if (wl12xx_dev_role_started(wlvif))
				wl12xx_stop_dev(wl, wlvif);

			ret = wl12xx_cmd_role_disable(wl, &wlvif->dev_role_id);
			if (ret < 0)
				goto deinit;
		}

		ret = wl12xx_cmd_role_disable(wl, &wlvif->role_id);
		if (ret < 0)
			goto deinit;

		wl1271_ps_elp_sleep(wl);
	}
deinit:
	/* clear all hlids (except system_hlid) */
	wlvif->dev_hlid = WL12XX_INVALID_LINK_ID;

	if (wlvif->bss_type == BSS_TYPE_STA_BSS ||
	    wlvif->bss_type == BSS_TYPE_IBSS) {
		wlvif->sta.hlid = WL12XX_INVALID_LINK_ID;
		wl12xx_free_rate_policy(wl, &wlvif->sta.basic_rate_idx);
		wl12xx_free_rate_policy(wl, &wlvif->sta.ap_rate_idx);
		wl12xx_free_rate_policy(wl, &wlvif->sta.p2p_rate_idx);
	} else {
		wlvif->ap.bcast_hlid = WL12XX_INVALID_LINK_ID;
		wlvif->ap.global_hlid = WL12XX_INVALID_LINK_ID;
		wl12xx_free_rate_policy(wl, &wlvif->ap.mgmt_rate_idx);
		wl12xx_free_rate_policy(wl, &wlvif->ap.bcast_rate_idx);
		for (i = 0; i < CONF_TX_MAX_AC_COUNT; i++)
			wl12xx_free_rate_policy(wl,
						&wlvif->ap.ucast_rate_idx[i]);
		wl1271_free_ap_keys(wl, wlvif);
	}

	dev_kfree_skb(wlvif->probereq);
	wlvif->probereq = NULL;
	memset(wl->rx_data_filters_status, 0,
	       sizeof(wl->rx_data_filters_status));
	wl12xx_tx_reset_wlvif(wl, wlvif);
	if (wl->last_wlvif == wlvif)
		wl->last_wlvif = NULL;
	list_del(&wlvif->list);
	memset(wlvif->ap.sta_hlid_map, 0, sizeof(wlvif->ap.sta_hlid_map));
	wlvif->role_id = WL12XX_INVALID_ROLE_ID;
	wlvif->dev_role_id = WL12XX_INVALID_ROLE_ID;

	if (wlvif->bss_type == BSS_TYPE_AP_BSS)
		wl->ap_count--;
	else
		wl->sta_count--;

	/* Last AP, have more stations. Configure according to STA. */
	if (wl->ap_count == 0 && wlvif->bss_type == BSS_TYPE_AP_BSS
	    && wl->sta_count) {
		/* Configure for ELP power saving */
		wl1271_acx_sleep_auth(wl, WL1271_PSM_ELP);
	}

	mutex_unlock(&wl->mutex);

	del_timer_sync(&wlvif->rx_streaming_timer);
	cancel_work_sync(&wlvif->rx_streaming_enable_work);
	cancel_work_sync(&wlvif->rx_streaming_disable_work);

	mutex_lock(&wl->mutex);
}

static void wl1271_op_remove_interface(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	struct wl12xx_vif *iter;
	bool cancel_recovery = true;

	mutex_lock(&wl->mutex);

	if (wl->state == WL1271_STATE_OFF ||
	    !test_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags))
		goto out;

	/*
	 * wl->vif can be null here if someone shuts down the interface
	 * just when hardware recovery has been started.
	 */
	wl12xx_for_each_wlvif(wl, iter) {
		if (iter != wlvif)
			continue;

		__wl1271_op_remove_interface(wl, vif, true);
		break;
	}
	WARN_ON(iter != wlvif);
	if (wl12xx_need_fw_change(hw, vif, wl->fw_type, false)) {
		wl12xx_force_active_psm(wl);
		set_bit(WL1271_FLAG_INTENDED_FW_RECOVERY, &wl->flags);
		wl12xx_queue_recovery_work(wl);
		cancel_recovery = false;
	}
out:
	mutex_unlock(&wl->mutex);
	if (cancel_recovery)
		cancel_work_sync(&wl->recovery_work);
}

static int wl12xx_op_change_interface(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      enum nl80211_iftype new_type, bool p2p)
{
	struct wl1271 *wl = hw->priv;
	int ret;

	set_bit(WL1271_FLAG_VIF_CHANGE_IN_PROGRESS, &wl->flags);
	wl1271_op_remove_interface(hw, vif);

	vif->type = new_type;
	vif->p2p = p2p;
	ret = wl1271_op_add_interface(hw, vif);

	clear_bit(WL1271_FLAG_VIF_CHANGE_IN_PROGRESS, &wl->flags);
	return ret;
}

static int wl1271_join(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			  bool set_assoc)
{
	int ret;
	bool is_ibss = (wlvif->bss_type == BSS_TYPE_IBSS);

	/*
	 * One of the side effects of the JOIN command is that is clears
	 * WPA/WPA2 keys from the chipset. Performing a JOIN while associated
	 * to a WPA/WPA2 access point will therefore kill the data-path.
	 * Currently the only valid scenario for JOIN during association
	 * is on roaming, in which case we will also be given new keys.
	 * Keep the below message for now, unless it starts bothering
	 * users who really like to roam a lot :)
	 */
	if (test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags))
		wl1271_info("JOIN while associated.");

	/* clear encryption type */
	wlvif->encryption_type = KEY_NONE;

	if (set_assoc)
		set_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags);

	if (is_ibss)
		ret = wl12xx_cmd_role_start_ibss(wl, wlvif);
	else
		ret = wl12xx_cmd_role_start_sta(wl, wlvif);
	if (ret < 0)
		goto out;

	if (!test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags))
		goto out;

	/*
	 * The join command disable the keep-alive mode, shut down its process,
	 * and also clear the template config, so we need to reset it all after
	 * the join. The acx_aid starts the keep-alive process, and the order
	 * of the commands below is relevant.
	 */
	ret = wl1271_acx_keep_alive_mode(wl, wlvif, true);
	if (ret < 0)
		goto out;

	ret = wl1271_acx_aid(wl, wlvif, wlvif->aid);
	if (ret < 0)
		goto out;

	ret = wl12xx_cmd_build_klv_null_data(wl, wlvif);
	if (ret < 0)
		goto out;

	ret = wl1271_acx_keep_alive_config(wl, wlvif,
					   CMD_TEMPL_KLV_IDX_NULL_DATA,
					   ACX_KEEP_ALIVE_TPL_VALID);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static int wl1271_unjoin(struct wl1271 *wl, struct wl12xx_vif *wlvif)
{
	int ret;

	if (test_and_clear_bit(WLVIF_FLAG_CS_PROGRESS, &wlvif->flags)) {
		struct ieee80211_vif *vif = wl12xx_wlvif_to_vif(wlvif);

		wl12xx_cmd_stop_channel_switch(wl);
		ieee80211_chswitch_done(vif, false);
	}

	/* to stop listening to a channel, we disconnect */
	ret = wl12xx_cmd_role_stop_sta(wl, wlvif);
	if (ret < 0)
		goto out;

	/* reset TX security counters on a clean disconnect */
	wlvif->tx_security_last_seq_lsb = 0;
	wlvif->tx_security_seq = 0;

out:
	return ret;
}

static void wl1271_set_band_rate(struct wl1271 *wl, struct wl12xx_vif *wlvif)
{
	wlvif->basic_rate_set = wlvif->bitrate_masks[wlvif->band];
	wlvif->rate_set = wlvif->basic_rate_set;
}

static int wl1271_sta_handle_idle(struct wl1271 *wl, struct wl12xx_vif *wlvif,
				  bool idle)
{
	int ret;
	bool cur_idle = !test_bit(WLVIF_FLAG_IN_USE, &wlvif->flags);

	if (idle == cur_idle)
		return 0;

	if (idle) {
		/* no need to croc if we weren't busy (e.g. during boot) */
		if (wl12xx_dev_role_started(wlvif)) {
			ret = wl12xx_stop_dev(wl, wlvif);
			if (ret < 0)
				goto out;
		}
		wlvif->rate_set =
			wl1271_tx_min_rate_get(wl, wlvif->basic_rate_set);
		ret = wl1271_acx_sta_rate_policies(wl, wlvif);
		if (ret < 0)
			goto out;
		ret = wl1271_acx_keep_alive_config(
			wl, wlvif, CMD_TEMPL_KLV_IDX_NULL_DATA,
			ACX_KEEP_ALIVE_TPL_INVALID);
		if (ret < 0)
			goto out;
		clear_bit(WLVIF_FLAG_IN_USE, &wlvif->flags);
	} else {
		/* The current firmware only supports sched_scan in idle */
		if (wl->sched_scanning) {
			wl1271_scan_sched_scan_stop(wl, wlvif);
			ieee80211_sched_scan_stopped(wl->hw);
		}

		ret = wl12xx_start_dev(wl, wlvif);
		if (ret < 0)
			goto out;
		set_bit(WLVIF_FLAG_IN_USE, &wlvif->flags);
	}

out:
	return ret;
}

static int wl12xx_config_vif(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			     struct ieee80211_conf *conf, u32 changed)
{
	bool is_ap = (wlvif->bss_type == BSS_TYPE_AP_BSS);
	int channel, ret;

	channel = ieee80211_frequency_to_channel(conf->channel->center_freq);

	/* if the channel changes while joined, join again */
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL &&
	    ((wlvif->band != conf->channel->band) ||
	     (wlvif->channel != channel))) {
		/* send all pending packets */
		ret = wl1271_tx_work_locked(wl);
		if (ret < 0)
			return ret;

		wlvif->band = conf->channel->band;
		wlvif->channel = channel;

		if (!is_ap) {
			/*
			 * FIXME: the mac80211 should really provide a fixed
			 * rate to use here. for now, just use the smallest
			 * possible rate for the band as a fixed rate for
			 * association frames and other control messages.
			 */
			if (!test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags))
				wl1271_set_band_rate(wl, wlvif);

			wlvif->basic_rate =
				wl1271_tx_min_rate_get(wl,
						       wlvif->basic_rate_set);
			ret = wl1271_acx_sta_rate_policies(wl, wlvif);
			if (ret < 0)
				wl1271_warning("rate policy for channel "
					       "failed %d", ret);

			if (test_bit(WLVIF_FLAG_STA_ASSOCIATED,
				     &wlvif->flags)) {
#if 0
				if (wl12xx_dev_role_started(wlvif)) {
					/* roaming */
					ret = wl12xx_croc(wl,
							  wlvif->dev_role_id);
					if (ret < 0)
						return ret;
				}
				ret = wl1271_join(wl, wlvif, false);
				if (ret < 0)
					wl1271_warning("cmd join on channel "
						       "failed %d", ret);
#endif
			} else {
				/*
				 * change the ROC channel. do it only if we are
				 * not idle. otherwise, CROC will be called
				 * anyway.
				 */
				if (wl12xx_dev_role_started(wlvif) &&
				    !(conf->flags & IEEE80211_CONF_IDLE)) {
					ret = wl12xx_stop_dev(wl, wlvif);
					if (ret < 0)
						return ret;

					ret = wl12xx_start_dev(wl, wlvif);
					if (ret < 0)
						return ret;
				}
			}
		}
	}

	if ((changed & IEEE80211_CONF_CHANGE_PS) && !is_ap) {

		if ((conf->flags & IEEE80211_CONF_PS) &&
		    test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags) &&
		    !test_bit(WLVIF_FLAG_IN_PS, &wlvif->flags)) {

			int ps_mode;
			char *ps_mode_str;

			if (wl->conf.conn.forced_ps) {
				ps_mode = STATION_POWER_SAVE_MODE;
				ps_mode_str = "forced";
			} else {
				ps_mode = STATION_AUTO_PS_MODE;
				ps_mode_str = "auto";
			}

			wl1271_debug(DEBUG_PSM, "%s ps enabled", ps_mode_str);

			ret = wl1271_ps_set_mode(wl, wlvif, ps_mode);

			if (ret < 0)
				wl1271_warning("enter %s ps failed %d",
					       ps_mode_str, ret);

		} else if (!(conf->flags & IEEE80211_CONF_PS) &&
			   test_bit(WLVIF_FLAG_IN_PS, &wlvif->flags)) {

			wl1271_debug(DEBUG_PSM, "auto ps disabled");

			ret = wl1271_ps_set_mode(wl, wlvif,
						 STATION_ACTIVE_MODE);
			if (ret < 0)
				wl1271_warning("exit auto ps failed %d", ret);
		}
	}

	if (conf->power_level != wlvif->power_level) {
		ret = wl1271_acx_tx_power(wl, wlvif, conf->power_level);
		if (ret < 0)
			return ret;

		wlvif->power_level = conf->power_level;
	}

	return 0;
}

static int wl1271_op_config(struct ieee80211_hw *hw, u32 changed)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif;
	struct ieee80211_conf *conf = &hw->conf;
	int channel, ret = 0;

	channel = ieee80211_frequency_to_channel(conf->channel->center_freq);

	wl1271_debug(DEBUG_MAC80211, "mac80211 config ch %d psm %s power %d %s"
		     " changed 0x%x",
		     channel,
		     conf->flags & IEEE80211_CONF_PS ? "on" : "off",
		     conf->power_level,
		     conf->flags & IEEE80211_CONF_IDLE ? "idle" : "in use",
			 changed);

	/*
	 * mac80211 will go to idle nearly immediately after transmitting some
	 * frames, such as the deauth. To make sure those frames reach the air,
	 * wait here until the TX queue is fully flushed.
	 */
	if ((changed & IEEE80211_CONF_CHANGE_CHANNEL) ||
	    ((changed & IEEE80211_CONF_CHANGE_IDLE) &&
	     (conf->flags & IEEE80211_CONF_IDLE)))
		wl1271_tx_flush(wl);

	mutex_lock(&wl->mutex);

	/* we support configuring the channel and band even while off */
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		wl->band = conf->channel->band;
		wl->channel = channel;
	}

	if (changed & IEEE80211_CONF_CHANGE_POWER)
		wl->power_level = conf->power_level;

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	/* configure each interface */
	wl12xx_for_each_wlvif(wl, wlvif) {
		ret = wl12xx_config_vif(wl, wlvif, conf, changed);
		if (ret < 0)
			goto out_sleep;
	}

out_sleep:
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

struct wl1271_filter_params {
	bool enabled;
	int mc_list_length;
	u8 mc_list[ACX_MC_ADDRESS_GROUP_MAX][ETH_ALEN];
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
static u64 wl1271_op_prepare_multicast(struct ieee80211_hw *hw,
				       struct netdev_hw_addr_list *mc_list)
#else
static u64 wl1271_op_prepare_multicast(struct ieee80211_hw *hw, int mc_count,
				       struct dev_addr_list *mc_list)
#endif
{
	struct wl1271_filter_params *fp;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	struct netdev_hw_addr *ha;
#else
	int i;
#endif
	struct wl1271 *wl = hw->priv;

	if (unlikely(wl->state == WL1271_STATE_OFF))
		return 0;

	fp = kzalloc(sizeof(*fp), GFP_ATOMIC);
	if (!fp) {
		wl1271_error("Out of memory setting filters.");
		return 0;
	}

	/* update multicast filtering parameters */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	fp->mc_list_length = 0;
	if (netdev_hw_addr_list_count(mc_list) > ACX_MC_ADDRESS_GROUP_MAX) {
#else
	fp->enabled = true;
	if (mc_count > ACX_MC_ADDRESS_GROUP_MAX) {
		mc_count = 0;
#endif
		fp->enabled = false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	} else {
		fp->enabled = true;
		netdev_hw_addr_list_for_each(ha, mc_list) {
#else
	}

	fp->mc_list_length = 0;
	for (i = 0; i < mc_count; i++) {
		if (mc_list->da_addrlen == ETH_ALEN) {
#endif
			memcpy(fp->mc_list[fp->mc_list_length],
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
					ha->addr, ETH_ALEN);
#else
			       mc_list->da_addr, ETH_ALEN);
#endif
			fp->mc_list_length++;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
		}
#else
		} else
			wl1271_warning("Unknown mc address length.");
		mc_list = mc_list->next;
#endif
	}

	return (u64)(unsigned long)fp;
}

#define WL1271_SUPPORTED_FILTERS (FIF_PROMISC_IN_BSS | \
				  FIF_ALLMULTI | \
				  FIF_FCSFAIL | \
				  FIF_BCN_PRBRESP_PROMISC | \
				  FIF_CONTROL | \
				  FIF_OTHER_BSS)

static void wl1271_op_configure_filter(struct ieee80211_hw *hw,
				       unsigned int changed,
				       unsigned int *total, u64 multicast)
{
	struct wl1271_filter_params *fp = (void *)(unsigned long)multicast;
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif;

	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 configure filter changed %x"
		     " total %x", changed, *total);

	mutex_lock(&wl->mutex);

	*total &= WL1271_SUPPORTED_FILTERS;
	changed &= WL1271_SUPPORTED_FILTERS;

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl12xx_for_each_wlvif(wl, wlvif) {
		if (wlvif->bss_type != BSS_TYPE_AP_BSS) {
			if (*total & FIF_ALLMULTI)
				ret = wl1271_acx_group_address_tbl(wl, wlvif,
								   false,
								   NULL, 0);
			else if (fp)
				ret = wl1271_acx_group_address_tbl(wl, wlvif,
							fp->enabled,
							fp->mc_list,
							fp->mc_list_length);
			if (ret < 0)
				goto out_sleep;
		}
	}

	/*
	 * the fw doesn't provide an api to configure the filters. instead,
	 * the filters configuration is based on the active roles / ROC
	 * state.
	 */

out_sleep:
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
	kfree(fp);
}

static int wl1271_record_ap_key(struct wl1271 *wl, struct wl12xx_vif *wlvif,
				u8 id, u8 key_type, u8 key_size,
				const u8 *key, u8 hlid, u32 tx_seq_32,
				u16 tx_seq_16)
{
	struct wl1271_ap_key *ap_key;
	int i;

	wl1271_debug(DEBUG_CRYPT, "record ap key id %d", (int)id);

	if (key_size > MAX_KEY_SIZE)
		return -EINVAL;

	/*
	 * Find next free entry in ap_keys. Also check we are not replacing
	 * an existing key.
	 */
	for (i = 0; i < MAX_NUM_KEYS; i++) {
		if (wlvif->ap.recorded_keys[i] == NULL)
			break;

		if (wlvif->ap.recorded_keys[i]->id == id) {
			wl1271_warning("trying to record key replacement");
			return -EINVAL;
		}
	}

	if (i == MAX_NUM_KEYS)
		return -EBUSY;

	ap_key = kzalloc(sizeof(*ap_key), GFP_KERNEL);
	if (!ap_key)
		return -ENOMEM;

	ap_key->id = id;
	ap_key->key_type = key_type;
	ap_key->key_size = key_size;
	memcpy(ap_key->key, key, key_size);
	ap_key->hlid = hlid;
	ap_key->tx_seq_32 = tx_seq_32;
	ap_key->tx_seq_16 = tx_seq_16;

	wlvif->ap.recorded_keys[i] = ap_key;
	return 0;
}

static void wl1271_free_ap_keys(struct wl1271 *wl, struct wl12xx_vif *wlvif)
{
	int i;

	for (i = 0; i < MAX_NUM_KEYS; i++) {
		kfree(wlvif->ap.recorded_keys[i]);
		wlvif->ap.recorded_keys[i] = NULL;
	}
}

static int wl1271_ap_init_hwenc(struct wl1271 *wl, struct wl12xx_vif *wlvif)
{
	int i, ret = 0;
	struct wl1271_ap_key *key;
	bool wep_key_added = false;

	for (i = 0; i < MAX_NUM_KEYS; i++) {
		u8 hlid;
		if (wlvif->ap.recorded_keys[i] == NULL)
			break;

		key = wlvif->ap.recorded_keys[i];
		hlid = key->hlid;
		if (hlid == WL12XX_INVALID_LINK_ID)
			hlid = wlvif->ap.bcast_hlid;

		ret = wl1271_cmd_set_ap_key(wl, wlvif, KEY_ADD_OR_REPLACE,
					    key->id, key->key_type,
					    key->key_size, key->key,
					    hlid, key->tx_seq_32,
					    key->tx_seq_16);
		if (ret < 0)
			goto out;

		if (key->key_type == KEY_WEP)
			wep_key_added = true;
	}

	if (wep_key_added) {
		ret = wl12xx_cmd_set_default_wep_key(wl, wlvif->default_key,
						     wlvif->ap.bcast_hlid);
		if (ret < 0)
			goto out;
	}

out:
	wl1271_free_ap_keys(wl, wlvif);
	return ret;
}

static int wl1271_set_key(struct wl1271 *wl, struct wl12xx_vif *wlvif,
		       u16 action, u8 id, u8 key_type,
		       u8 key_size, const u8 *key, u32 tx_seq_32,
		       u16 tx_seq_16, struct ieee80211_sta *sta)
{
	int ret;
	bool is_ap = (wlvif->bss_type == BSS_TYPE_AP_BSS);

	if (is_ap) {
		struct wl1271_station *wl_sta;
		u8 hlid;

		if (sta) {
			wl_sta = (struct wl1271_station *)sta->drv_priv;
			hlid = wl_sta->hlid;
		} else {
			hlid = wlvif->ap.bcast_hlid;
		}

		if (!test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags)) {
			/*
			 * We do not support removing keys after AP shutdown.
			 * Pretend we do to make mac80211 happy.
			 */
			if (action != KEY_ADD_OR_REPLACE)
				return 0;

			ret = wl1271_record_ap_key(wl, wlvif, id,
					     key_type, key_size,
					     key, hlid, tx_seq_32,
					     tx_seq_16);
		} else {
			ret = wl1271_cmd_set_ap_key(wl, wlvif, action,
					     id, key_type, key_size,
					     key, hlid, tx_seq_32,
					     tx_seq_16);
		}

		if (ret < 0)
			return ret;
	} else {
		const u8 *addr;
		static const u8 bcast_addr[ETH_ALEN] = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff
		};

		addr = sta ? sta->addr : bcast_addr;

		if (is_zero_ether_addr(addr)) {
			/* We dont support TX only encryption */
			return -EOPNOTSUPP;
		}

		/* The wl1271 does not allow to remove unicast keys - they
		   will be cleared automatically on next CMD_JOIN. Ignore the
		   request silently, as we dont want the mac80211 to emit
		   an error message. */
		if (action == KEY_REMOVE && !is_broadcast_ether_addr(addr))
			return 0;

		/* don't remove key if hlid was already deleted */
		if (action == KEY_REMOVE &&
		    wlvif->sta.hlid == WL12XX_INVALID_LINK_ID)
			return 0;

		ret = wl1271_cmd_set_sta_key(wl, wlvif, action,
					     id, key_type, key_size,
					     key, addr, tx_seq_32,
					     tx_seq_16);
		if (ret < 0)
			return ret;

	}

	return 0;
}

static int wl1271_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			     struct ieee80211_vif *vif,
			     struct ieee80211_sta *sta,
			     struct ieee80211_key_conf *key_conf)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret;
	u32 tx_seq_32 = 0;
	u16 tx_seq_16 = 0;
	u8 key_type;

	wl1271_debug(DEBUG_MAC80211, "mac80211 set key");

	wl1271_debug(DEBUG_CRYPT, "CMD: 0x%x sta: %p", cmd, sta);
	wl1271_debug(DEBUG_CRYPT, "Key: algo:0x%x, id:%d, len:%d flags 0x%x",
		     key_conf->cipher, key_conf->keyidx,
		     key_conf->keylen, key_conf->flags);
	wl1271_dump(DEBUG_CRYPT, "KEY: ", key_conf->key, key_conf->keylen);

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF)) {
		ret = -EAGAIN;
		goto out_unlock;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out_unlock;

	switch (key_conf->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		key_type = KEY_WEP;

		key_conf->hw_key_idx = key_conf->keyidx;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		key_type = KEY_TKIP;

		key_conf->hw_key_idx = key_conf->keyidx;
		tx_seq_32 = WL1271_TX_SECURITY_HI32(wlvif->tx_security_seq);
		tx_seq_16 = WL1271_TX_SECURITY_LO16(wlvif->tx_security_seq);
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		key_type = KEY_AES;

		key_conf->flags |= IEEE80211_KEY_FLAG_PUT_IV_SPACE;
		tx_seq_32 = WL1271_TX_SECURITY_HI32(wlvif->tx_security_seq);
		tx_seq_16 = WL1271_TX_SECURITY_LO16(wlvif->tx_security_seq);
		break;
	case WL1271_CIPHER_SUITE_GEM:
		key_type = KEY_GEM;
		tx_seq_32 = WL1271_TX_SECURITY_HI32(wlvif->tx_security_seq);
		tx_seq_16 = WL1271_TX_SECURITY_LO16(wlvif->tx_security_seq);
		break;
	default:
		wl1271_error("Unknown key algo 0x%x", key_conf->cipher);

		ret = -EOPNOTSUPP;
		goto out_sleep;
	}

	switch (cmd) {
	case SET_KEY:
		ret = wl1271_set_key(wl, wlvif, KEY_ADD_OR_REPLACE,
				 key_conf->keyidx, key_type,
				 key_conf->keylen, key_conf->key,
				 tx_seq_32, tx_seq_16, sta);
		if (ret < 0) {
			wl1271_error("Could not add or replace key");
			goto out_sleep;
		}

		/*
		 * reconfiguring arp response if the unicast (or common)
		 * encryption key type was changed
		 */
		if (wlvif->bss_type == BSS_TYPE_STA_BSS &&
		    (sta || key_type == KEY_WEP) &&
		    wlvif->encryption_type != key_type) {
			wlvif->encryption_type = key_type;
			ret = wl1271_cmd_build_arp_rsp(wl, wlvif);
			if (ret < 0) {
				wl1271_warning("build arp rsp failed: %d", ret);
				goto out_sleep;
			}
		}
		break;

	case DISABLE_KEY:
		ret = wl1271_set_key(wl, wlvif, KEY_REMOVE,
				     key_conf->keyidx, key_type,
				     key_conf->keylen, key_conf->key,
				     0, 0, sta);
		if (ret < 0) {
			wl1271_error("Could not remove key");
			goto out_sleep;
		}
		break;

	default:
		wl1271_error("Unsupported key cmd 0x%x", cmd);
		ret = -EOPNOTSUPP;
		break;
	}

out_sleep:
	wl1271_ps_elp_sleep(wl);

out_unlock:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl1271_op_set_default_key_idx(struct ieee80211_hw *hw,
					 struct ieee80211_vif *vif,
					 int key_idx)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret = 0;

	wl1271_debug(DEBUG_MAC80211, "mac80211 set default key idx %d",
		     key_idx);

	mutex_lock(&wl->mutex);

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out_unlock;

	wlvif->default_key = key_idx;

	/* the default WEP key needs to be configured at least once */
	if (wlvif->encryption_type == KEY_WEP) {
		ret = wl12xx_cmd_set_default_wep_key(wl,
				key_idx,
				wlvif->sta.hlid);
		if (ret < 0)
			goto out_sleep;
	}

out_sleep:
	wl1271_ps_elp_sleep(wl);

out_unlock:
	mutex_unlock(&wl->mutex);
	return ret;
}

static int wl1271_op_hw_scan(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct cfg80211_scan_request *req)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret;
	u8 *ssid = NULL;
	size_t len = 0;

	wl1271_debug(DEBUG_MAC80211, "mac80211 hw scan");

	if (req->n_ssids) {
		ssid = req->ssids[0].ssid;
		len = req->ssids[0].ssid_len;
	}

	mutex_lock(&wl->mutex);

	if (!test_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags)) {
		/*
		 * We cannot return -EBUSY here because cfg80211 will expect
		 * a call to ieee80211_scan_completed if we do - in this case
		 * there won't be any call.
		 */
		ret = -EAGAIN;
		goto out;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	/* fail if there is any role in ROC */
	if (find_first_bit(wl->roc_map, WL12XX_MAX_ROLES) < WL12XX_MAX_ROLES) {
		/* don't allow scanning right now */
		ret = -EBUSY;
		goto out_sleep;
	}

	ret = wl1271_scan(hw->priv, vif, ssid, len, req);
out_sleep:
	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static void wl1271_op_cancel_hw_scan(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif)
{
	struct wl1271 *wl = hw->priv;
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 cancel hw scan");

	mutex_lock(&wl->mutex);

	if (wl->state == WL1271_STATE_OFF)
		goto out;

	if (wl->scan.state == WL1271_SCAN_STATE_IDLE)
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	if (wl->scan.state != WL1271_SCAN_STATE_DONE) {
		ret = wl1271_scan_stop(wl);
		if (ret < 0)
			goto out_sleep;
	}
	wl->scan.state = WL1271_SCAN_STATE_IDLE;
	memset(wl->scan.scanned_ch, 0, sizeof(wl->scan.scanned_ch));
	wl->scan_vif = NULL;
	wl->scan.req = NULL;
	ieee80211_scan_completed(wl->hw, true);

out_sleep:
	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);

	cancel_delayed_work_sync(&wl->scan_complete_work);
}

static int wl1271_op_sched_scan_start(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      struct cfg80211_sched_scan_request *req,
				      struct ieee80211_sched_scan_ies *ies)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret;

	wl1271_debug(DEBUG_MAC80211, "wl1271_op_sched_scan_start");

	mutex_lock(&wl->mutex);

	if (wl->state == WL1271_STATE_OFF) {
		ret = -EAGAIN;
		goto out;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_scan_sched_scan_config(wl, wlvif, req, ies);
	if (ret < 0)
		goto out_sleep;

	ret = wl1271_scan_sched_scan_start(wl, wlvif);
	if (ret < 0)
		goto out_sleep;

	wl->sched_scanning = true;

out_sleep:
	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
	return ret;
}

static void wl1271_op_sched_scan_stop(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret;

	wl1271_debug(DEBUG_MAC80211, "wl1271_op_sched_scan_stop");

	mutex_lock(&wl->mutex);

	if (wl->state == WL1271_STATE_OFF)
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1271_scan_sched_scan_stop(wl, wlvif);

	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
}

static int wl1271_op_set_frag_threshold(struct ieee80211_hw *hw, u32 value)
{
	struct wl1271 *wl = hw->priv;
	int ret = 0;

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF)) {
		ret = -EAGAIN;
		goto out;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_acx_frag_threshold(wl, value);
	if (ret < 0)
		wl1271_warning("wl1271_op_set_frag_threshold failed: %d", ret);

	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl1271_op_set_rts_threshold(struct ieee80211_hw *hw, u32 value)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif;
	int ret = 0;

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF)) {
		ret = -EAGAIN;
		goto out;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl12xx_for_each_wlvif(wl, wlvif) {
		ret = wl1271_acx_rts_threshold(wl, wlvif, value);
		if (ret < 0)
			wl1271_warning("set rts threshold failed: %d", ret);
	}
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl1271_ssid_set(struct ieee80211_vif *vif, struct sk_buff *skb,
			    int offset)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	u8 ssid_len;
	const u8 *ptr = cfg80211_find_ie(WLAN_EID_SSID, skb->data + offset,
					 skb->len - offset);

	if (!ptr) {
		wl1271_error("No SSID in IEs!");
		return -ENOENT;
	}

	ssid_len = ptr[1];
	if (ssid_len > IEEE80211_MAX_SSID_LEN) {
		wl1271_error("SSID is too long!");
		return -EINVAL;
	}

	wlvif->ssid_len = ssid_len;
	memcpy(wlvif->ssid, ptr+2, ssid_len);
	return 0;
}

static void wl12xx_remove_ie(struct sk_buff *skb, u8 eid, int ieoffset)
{
	int len;
	const u8 *next, *end = skb->data + skb->len;
	u8 *ie = (u8 *)cfg80211_find_ie(eid, skb->data + ieoffset,
					skb->len - ieoffset);
	if (!ie)
		return;
	len = ie[1] + 2;
	next = ie + len;
	memmove(ie, next, end - next);
	skb_trim(skb, skb->len - len);
}

static void wl12xx_remove_vendor_ie(struct sk_buff *skb,
					    unsigned int oui, u8 oui_type,
					    int ieoffset)
{
	int len;
	const u8 *next, *end = skb->data + skb->len;
	u8 *ie = (u8 *)cfg80211_find_vendor_ie(oui, oui_type,
					       skb->data + ieoffset,
					       skb->len - ieoffset);
	if (!ie)
		return;
	len = ie[1] + 2;
	next = ie + len;
	memmove(ie, next, end - next);
	skb_trim(skb, skb->len - len);
}

static int wl1271_ap_set_probe_resp_tmpl(struct wl1271 *wl, u32 rates,
					 struct ieee80211_vif *vif)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	struct sk_buff *skb;
	int ret;

	skb = ieee80211_proberesp_get(wl->hw, vif);
	if (!skb)
		return -EOPNOTSUPP;

	ret = wl1271_cmd_template_set(wl, wlvif->role_id,
				      CMD_TEMPL_AP_PROBE_RESPONSE,
				      skb->data,
				      skb->len, 0,
				      rates);

	dev_kfree_skb(skb);
	return ret;
}

static int wl1271_ap_set_probe_resp_tmpl_legacy(struct wl1271 *wl,
					     struct ieee80211_vif *vif,
					     u8 *probe_rsp_data,
					     size_t probe_rsp_len,
					     u32 rates)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	struct ieee80211_bss_conf *bss_conf = &vif->bss_conf;
	u8 probe_rsp_templ[WL1271_CMD_TEMPL_MAX_SIZE];
	int ssid_ie_offset, ie_offset, templ_len;
	const u8 *ptr;

	/* no need to change probe response if the SSID is set correctly */
	if (wlvif->ssid_len > 0)
		return wl1271_cmd_template_set(wl, wlvif->role_id,
					       CMD_TEMPL_AP_PROBE_RESPONSE,
					       probe_rsp_data,
					       probe_rsp_len, 0,
					       rates);

	if (probe_rsp_len + bss_conf->ssid_len > WL1271_CMD_TEMPL_MAX_SIZE) {
		wl1271_error("probe_rsp template too big");
		return -EINVAL;
	}

	/* start searching from IE offset */
	ie_offset = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);

	ptr = cfg80211_find_ie(WLAN_EID_SSID, probe_rsp_data + ie_offset,
			       probe_rsp_len - ie_offset);
	if (!ptr) {
		wl1271_error("No SSID in beacon!");
		return -EINVAL;
	}

	ssid_ie_offset = ptr - probe_rsp_data;
	ptr += (ptr[1] + 2);

	memcpy(probe_rsp_templ, probe_rsp_data, ssid_ie_offset);

	/* insert SSID from bss_conf */
	probe_rsp_templ[ssid_ie_offset] = WLAN_EID_SSID;
	probe_rsp_templ[ssid_ie_offset + 1] = bss_conf->ssid_len;
	memcpy(probe_rsp_templ + ssid_ie_offset + 2,
	       bss_conf->ssid, bss_conf->ssid_len);
	templ_len = ssid_ie_offset + 2 + bss_conf->ssid_len;

	memcpy(probe_rsp_templ + ssid_ie_offset + 2 + bss_conf->ssid_len,
	       ptr, probe_rsp_len - (ptr - probe_rsp_data));
	templ_len += probe_rsp_len - (ptr - probe_rsp_data);

	return wl1271_cmd_template_set(wl, wlvif->role_id,
				       CMD_TEMPL_AP_PROBE_RESPONSE,
				       probe_rsp_templ,
				       templ_len, 0,
				       rates);
}

static int wl1271_bss_erp_info_changed(struct wl1271 *wl,
				       struct ieee80211_vif *vif,
				       struct ieee80211_bss_conf *bss_conf,
				       u32 changed)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret = 0;

	if (changed & BSS_CHANGED_ERP_SLOT) {
		if (bss_conf->use_short_slot)
			ret = wl1271_acx_slot(wl, wlvif, SLOT_TIME_SHORT);
		else
			ret = wl1271_acx_slot(wl, wlvif, SLOT_TIME_LONG);
		if (ret < 0) {
			wl1271_warning("Set slot time failed %d", ret);
			goto out;
		}
	}

	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		if (bss_conf->use_short_preamble)
			wl1271_acx_set_preamble(wl, wlvif, ACX_PREAMBLE_SHORT);
		else
			wl1271_acx_set_preamble(wl, wlvif, ACX_PREAMBLE_LONG);
	}

	if (changed & BSS_CHANGED_ERP_CTS_PROT) {
		if (bss_conf->use_cts_prot)
			ret = wl1271_acx_cts_protect(wl, wlvif,
						     CTSPROTECT_ENABLE);
		else
			ret = wl1271_acx_cts_protect(wl, wlvif,
						     CTSPROTECT_DISABLE);
		if (ret < 0) {
			wl1271_warning("Set ctsprotect failed %d", ret);
			goto out;
		}
	}

out:
	return ret;
}

static int wl1271_bss_beacon_info_changed(struct wl1271 *wl,
					  struct ieee80211_vif *vif,
					  struct ieee80211_bss_conf *bss_conf,
					  u32 changed)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	bool is_ap = (wlvif->bss_type == BSS_TYPE_AP_BSS);
	int ret = 0;

	if ((changed & BSS_CHANGED_BEACON_INT)) {
		wl1271_debug(DEBUG_MASTER, "beacon interval updated: %d",
			bss_conf->beacon_int);

		wlvif->beacon_int = bss_conf->beacon_int;
	}

	if ((changed & BSS_CHANGED_AP_PROBE_RESP) && is_ap) {
		u32 rate = wl1271_tx_min_rate_get(wl, wlvif->basic_rate_set);
		if (!wl1271_ap_set_probe_resp_tmpl(wl, rate, vif)) {
			wl1271_debug(DEBUG_AP, "probe response updated");
			set_bit(WLVIF_FLAG_AP_PROBE_RESP_SET, &wlvif->flags);
		}
	}

	if ((changed & BSS_CHANGED_BEACON)) {
		struct ieee80211_hdr *hdr;
		u32 min_rate;
		int ieoffset = offsetof(struct ieee80211_mgmt,
					u.beacon.variable);
		struct sk_buff *beacon = ieee80211_beacon_get(wl->hw, vif);
		u16 tmpl_id;

		if (!beacon) {
			ret = -EINVAL;
			goto out;
		}

		wl1271_debug(DEBUG_MASTER, "beacon updated");

		ret = wl1271_ssid_set(vif, beacon, ieoffset);
		if (ret < 0) {
			dev_kfree_skb(beacon);
			goto out;
		}
		min_rate = wl1271_tx_min_rate_get(wl, wlvif->basic_rate_set);
		tmpl_id = is_ap ? CMD_TEMPL_AP_BEACON :
				  CMD_TEMPL_BEACON;
		ret = wl1271_cmd_template_set(wl, wlvif->role_id, tmpl_id,
					      beacon->data,
					      beacon->len, 0,
					      min_rate);
		if (ret < 0) {
			dev_kfree_skb(beacon);
			goto out;
		}

		/*
		 * In case we already have a probe-resp beacon set explicitly
		 * by usermode, don't use the beacon data.
		 */
		if (test_bit(WLVIF_FLAG_AP_PROBE_RESP_SET, &wlvif->flags))
			goto end_bcn;

		/* remove TIM ie from probe response */
		wl12xx_remove_ie(beacon, WLAN_EID_TIM, ieoffset);

		/*
		 * remove p2p ie from probe response.
		 * the fw reponds to probe requests that don't include
		 * the p2p ie. probe requests with p2p ie will be passed,
		 * and will be responded by the supplicant (the spec
		 * forbids including the p2p ie when responding to probe
		 * requests that didn't include it).
		 */
		wl12xx_remove_vendor_ie(beacon, WLAN_OUI_WFA,
					WLAN_OUI_TYPE_WFA_P2P, ieoffset);

		hdr = (struct ieee80211_hdr *) beacon->data;
		hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
						 IEEE80211_STYPE_PROBE_RESP);
		if (is_ap)
			ret = wl1271_ap_set_probe_resp_tmpl_legacy(wl, vif,
						beacon->data,
						beacon->len,
						min_rate);
		else
			ret = wl1271_cmd_template_set(wl, wlvif->role_id,
						CMD_TEMPL_PROBE_RESPONSE,
						beacon->data,
						beacon->len, 0,
						min_rate);
end_bcn:
		dev_kfree_skb(beacon);
		if (ret < 0)
			goto out;
	}

out:
	if (ret != 0)
		wl1271_error("beacon info change failed: %d", ret);
	return ret;
}

/* AP mode changes */
static void wl1271_bss_info_changed_ap(struct wl1271 *wl,
				       struct ieee80211_vif *vif,
				       struct ieee80211_bss_conf *bss_conf,
				       u32 changed)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret = 0;

	if ((changed & BSS_CHANGED_BASIC_RATES)) {
		u32 rates = bss_conf->basic_rates;

		wlvif->basic_rate_set = wl1271_tx_enabled_rates_get(wl, rates,
								 wlvif->band);
		wlvif->basic_rate = wl1271_tx_min_rate_get(wl,
							wlvif->basic_rate_set);

		ret = wl1271_init_ap_rates(wl, wlvif);
		if (ret < 0) {
			wl1271_error("AP rate policy change failed %d", ret);
			goto out;
		}

		ret = wl1271_ap_init_templates(wl, vif);
		if (ret < 0)
			goto out;
	}

	ret = wl1271_bss_beacon_info_changed(wl, vif, bss_conf, changed);
	if (ret < 0)
		goto out;

	if ((changed & BSS_CHANGED_BEACON_ENABLED)) {
		if (bss_conf->enable_beacon) {
			if (!test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags)) {
				ret = wl12xx_cmd_role_start_ap(wl, wlvif);
				if (ret < 0)
					goto out;

				ret = wl1271_ap_init_hwenc(wl, wlvif);
				if (ret < 0)
					goto out;

				set_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags);
				wl1271_debug(DEBUG_AP, "started AP");
			}
		} else {
			if (test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags)) {
				ret = wl12xx_cmd_role_stop_ap(wl, wlvif);
				if (ret < 0)
					goto out;

				clear_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags);
				clear_bit(WLVIF_FLAG_AP_PROBE_RESP_SET,
					  &wlvif->flags);
				wl1271_debug(DEBUG_AP, "stopped AP");
			}
		}
	}

	ret = wl1271_bss_erp_info_changed(wl, vif, bss_conf, changed);
	if (ret < 0)
		goto out;

	/* Handle HT information change */
	if ((changed & BSS_CHANGED_HT) &&
	    (bss_conf->channel_type != NL80211_CHAN_NO_HT)) {
		ret = wl1271_acx_set_ht_information(wl, wlvif,
					bss_conf->ht_operation_mode);
		if (ret < 0) {
			wl1271_warning("Set ht information failed %d", ret);
			goto out;
		}
	}

	if (!list_empty(&wl->peers_list) &&
	    test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags)) {
		struct ap_peers *peer, *next;
		enum ieee80211_sta_state state = IEEE80211_STA_NONE;

		list_for_each_entry_safe(peer, next, &wl->peers_list, list) {
			wl1271_op_sta_add_locked(peer->hw, peer->vif,
						 &peer->sta);
			while (state < peer->sta.state)
				wl12xx_update_sta_state(wl, &peer->sta,
							++state);

			wl1271_debug(DEBUG_AP, "add sta %pM", peer->sta.addr);
			list_del(&peer->list);
			kfree(peer);
		}
	}


out:
	return;
}

/* STA/IBSS mode changes */
static void wl1271_bss_info_changed_sta(struct wl1271 *wl,
					struct ieee80211_vif *vif,
					struct ieee80211_bss_conf *bss_conf,
					u32 changed)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	bool do_join = false, set_assoc = false;
	bool is_ibss = (wlvif->bss_type == BSS_TYPE_IBSS);
	bool ibss_joined = false;
	u32 sta_rate_set = 0;
	int ret;
	struct ieee80211_sta *sta;
	bool sta_exists = false;
	struct ieee80211_sta_ht_cap sta_ht_cap;

	if (is_ibss) {
		ret = wl1271_bss_beacon_info_changed(wl, vif, bss_conf,
						     changed);
		if (ret < 0)
			goto out;
	}

	if (changed & BSS_CHANGED_IBSS) {
		if (bss_conf->ibss_joined) {
			set_bit(WLVIF_FLAG_IBSS_JOINED, &wlvif->flags);
			ibss_joined = true;
		} else {
			if (test_and_clear_bit(WLVIF_FLAG_IBSS_JOINED,
					       &wlvif->flags))
				wl1271_unjoin(wl, wlvif);
		}
	}

	if ((changed & BSS_CHANGED_BEACON_INT) && ibss_joined)
		do_join = true;

	/* Need to update the SSID (for filtering etc) */
	if ((changed & BSS_CHANGED_BEACON) && ibss_joined)
		do_join = true;

	if ((changed & BSS_CHANGED_BEACON_ENABLED) && ibss_joined) {
		wl1271_debug(DEBUG_ADHOC, "ad-hoc beaconing: %s",
			     bss_conf->enable_beacon ? "enabled" : "disabled");

		do_join = true;
	}

	if (changed & BSS_CHANGED_IDLE && !is_ibss) {
		ret = wl1271_sta_handle_idle(wl, wlvif, bss_conf->idle);
		if (ret < 0)
			wl1271_warning("idle mode change failed %d", ret);
	}

	if ((changed & BSS_CHANGED_CQM)) {
		bool enable = false;
		if (bss_conf->cqm_rssi_thold)
			enable = true;
		ret = wl1271_acx_rssi_snr_trigger(wl, wlvif, enable,
						  bss_conf->cqm_rssi_thold,
						  bss_conf->cqm_rssi_hyst);
		if (ret < 0)
			goto out;
		wlvif->rssi_thold = bss_conf->cqm_rssi_thold;
	}

	if (changed & BSS_CHANGED_BSSID)
		if (!is_zero_ether_addr(bss_conf->bssid)) {
			ret = wl12xx_cmd_build_null_data(wl, wlvif);
			if (ret < 0)
				goto out;

			ret = wl1271_build_qos_null_data(wl, vif);
			if (ret < 0)
				goto out;

			/* Need to update the BSSID (for filtering etc) */
			do_join = true;
		}

	if (changed & (BSS_CHANGED_ASSOC | BSS_CHANGED_HT)) {
		rcu_read_lock();
		sta = ieee80211_find_sta(vif, bss_conf->bssid);
		if (!sta)
			goto sta_not_found;

		/* save the supp_rates of the ap */
		sta_rate_set = sta->supp_rates[wl->hw->conf.channel->band];
		if (sta->ht_cap.ht_supported)
			sta_rate_set |=
			    (sta->ht_cap.mcs.rx_mask[0] << HW_HT_RATES_OFFSET);
		sta_ht_cap = sta->ht_cap;
		sta_exists = true;

sta_not_found:
		rcu_read_unlock();
	}

	if ((changed & BSS_CHANGED_ASSOC)) {
		if (bss_conf->assoc) {
			u32 rates;
			int ieoffset;
			wlvif->aid = bss_conf->aid;
			wlvif->beacon_int = bss_conf->beacon_int;
			set_assoc = true;

			/*
			 * use basic rates from AP, and determine lowest rate
			 * to use with control frames.
			 */
			rates = bss_conf->basic_rates;
			wlvif->basic_rate_set =
				wl1271_tx_enabled_rates_get(wl, rates,
							    wlvif->band);
			wlvif->basic_rate =
				wl1271_tx_min_rate_get(wl,
						       wlvif->basic_rate_set);
			if (sta_rate_set)
				wlvif->rate_set =
					wl1271_tx_enabled_rates_get(wl,
								sta_rate_set,
								wlvif->band);
			ret = wl1271_acx_sta_rate_policies(wl, wlvif);
			if (ret < 0)
				goto out;

			/*
			 * with wl1271, we don't need to update the
			 * beacon_int and dtim_period, because the firmware
			 * updates it by itself when the first beacon is
			 * received after a join.
			 */
			ret = wl1271_cmd_build_ps_poll(wl, wlvif, wlvif->aid);
			if (ret < 0)
				goto out;

			/*
			 * Get a template for hardware connection maintenance
			 */
			dev_kfree_skb(wlvif->probereq);
			wlvif->probereq = wl1271_cmd_build_ap_probe_req(wl,
									wlvif,
									NULL);
			ieoffset = offsetof(struct ieee80211_mgmt,
					    u.probe_req.variable);
			wl1271_ssid_set(vif, wlvif->probereq, ieoffset);

			/* enable the connection monitoring feature */
			wlvif->sta.first_bcn_loss = 0;
			wlvif->sta.last_bcn_loss = 0;
			ret = wl1271_acx_conn_monit_params(wl, wlvif, true);
			if (ret < 0)
				goto out;
		} else {
			/* use defaults when not associated */
			bool was_assoc =
			    !!test_and_clear_bit(WLVIF_FLAG_STA_ASSOCIATED,
						 &wlvif->flags);
			bool was_ifup =
			    !!test_and_clear_bit(WLVIF_FLAG_STA_STATE_SENT,
						 &wlvif->flags);
			wlvif->aid = 0;

			/* free probe-request template */
			dev_kfree_skb(wlvif->probereq);
			wlvif->probereq = NULL;

			/* revert back to minimum rates for the current band */
			wl1271_set_band_rate(wl, wlvif);
			wlvif->basic_rate =
				wl1271_tx_min_rate_get(wl,
						       wlvif->basic_rate_set);
			ret = wl1271_acx_sta_rate_policies(wl, wlvif);
			if (ret < 0)
				goto out;

			/* disable connection monitor features */
			ret = wl1271_acx_conn_monit_params(wl, wlvif, false);

			/* Disable the keep-alive feature */
			ret = wl1271_acx_keep_alive_mode(wl, wlvif, false);
			if (ret < 0)
				goto out;

			/* restore the bssid filter and go to dummy bssid */
			if (was_assoc) {
				/*
				 * we might have to disable roc, if there was
				 * no IF_OPER_UP notification.
				 */
				if (!was_ifup) {
					ret = wl12xx_croc(wl, wlvif->role_id);
					if (ret < 0)
						goto out;
				}
				/*
				 * (we also need to disable roc in case of
				 * roaming on the same channel. until we will
				 * have a better flow...)
				 */
				if (test_bit(wlvif->dev_role_id, wl->roc_map)) {
					ret = wl12xx_croc(wl,
							  wlvif->dev_role_id);
					if (ret < 0)
						goto out;
				}

				wl1271_unjoin(wl, wlvif);
				if (!bss_conf->idle)
					wl12xx_start_dev(wl, wlvif);
			}
		}
	}

	if (changed & BSS_CHANGED_IBSS) {
		wl1271_debug(DEBUG_ADHOC, "ibss_joined: %d",
			     bss_conf->ibss_joined);

		if (bss_conf->ibss_joined) {
			u32 rates = bss_conf->basic_rates;
			wlvif->basic_rate_set =
				wl1271_tx_enabled_rates_get(wl, rates,
							    wlvif->band);
			wlvif->basic_rate =
				wl1271_tx_min_rate_get(wl,
						       wlvif->basic_rate_set);

			/* by default, use 11b + OFDM rates */
			wlvif->rate_set = CONF_TX_IBSS_DEFAULT_RATES;
			ret = wl1271_acx_sta_rate_policies(wl, wlvif);
			if (ret < 0)
				goto out;
		}
	}

	ret = wl1271_bss_erp_info_changed(wl, vif, bss_conf, changed);
	if (ret < 0)
		goto out;

	if (do_join) {
		ret = wl1271_join(wl, wlvif, set_assoc);
		if (ret < 0) {
			wl1271_warning("cmd join failed %d", ret);
			goto out;
		}

		/* ROC until connected (after EAPOL exchange) */
		if (!is_ibss) {
			ret = wl12xx_roc(wl, wlvif, wlvif->role_id);
			if (ret < 0)
				goto out;

			wl1271_check_operstate(wl, wlvif,
					       ieee80211_get_operstate(vif));
		}
		/*
		 * stop device role if started (we might already be in
		 * STA/IBSS role).
		 */
		if (wl12xx_dev_role_started(wlvif)) {
			ret = wl12xx_stop_dev(wl, wlvif);
			if (ret < 0)
				goto out;
		}
	}

	/* Handle new association with HT. Do this after join. */
	if (sta_exists) {
		if ((changed & BSS_CHANGED_HT) &&
		    (bss_conf->channel_type != NL80211_CHAN_NO_HT)) {
			ret = wl1271_acx_set_ht_capabilities(wl,
							     &sta_ht_cap,
							     true,
							     wlvif->sta.hlid);
			if (ret < 0) {
				wl1271_warning("Set ht cap true failed %d",
					       ret);
				goto out;
			}
		}
		/* handle new association without HT and disassociation */
		else if (changed & BSS_CHANGED_ASSOC) {
			ret = wl1271_acx_set_ht_capabilities(wl,
							     &sta_ht_cap,
							     false,
							     wlvif->sta.hlid);
			if (ret < 0) {
				wl1271_warning("Set ht cap false failed %d",
					       ret);
				goto out;
			}
		}
	}

	/* Handle HT information change. Done after join. */
	if ((changed & BSS_CHANGED_HT) &&
	    (bss_conf->channel_type != NL80211_CHAN_NO_HT)) {
		ret = wl1271_acx_set_ht_information(wl, wlvif,
					bss_conf->ht_operation_mode);
		if (ret < 0) {
			wl1271_warning("Set ht information failed %d", ret);
			goto out;
		}
	}

	/* Handle arp filtering. Done after join. */
	if ((changed & BSS_CHANGED_ARP_FILTER) ||
	    (!is_ibss && (changed & BSS_CHANGED_QOS))) {
		__be32 addr = bss_conf->arp_addr_list[0];
		wlvif->sta.qos = bss_conf->qos;
		WARN_ON(wlvif->bss_type != BSS_TYPE_STA_BSS);

		if (bss_conf->arp_addr_cnt == 1 &&
		    bss_conf->arp_filter_enabled) {
			wlvif->ip_addr = addr;
			/*
			 * The template should have been configured only upon
			 * association. however, it seems that the correct ip
			 * isn't being set (when sending), so we have to
			 * reconfigure the template upon every ip change.
			 */
			ret = wl1271_cmd_build_arp_rsp(wl, wlvif);
			if (ret < 0) {
				wl1271_warning("build arp rsp failed: %d", ret);
				goto out;
			}

			ret = wl1271_acx_arp_ip_filter(wl, wlvif,
				(ACX_ARP_FILTER_ARP_FILTERING |
				 ACX_ARP_FILTER_AUTO_ARP),
				addr);
		} else {
			wlvif->ip_addr = 0;
			ret = wl1271_acx_arp_ip_filter(wl, wlvif, 0, addr);
		}

		if (ret < 0)
			goto out;
	}

out:
	return;
}

static void wl1271_op_bss_info_changed(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_bss_conf *bss_conf,
				       u32 changed)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	bool is_ap = (wlvif->bss_type == BSS_TYPE_AP_BSS);
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 bss info changed 0x%x",
		     (int)changed);

	if ((changed & BSS_CHANGED_BEACON_ENABLED) &&
	    !bss_conf->enable_beacon)
		wl1271_tx_flush(wl);

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	if (unlikely(!test_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags)))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	if (is_ap)
		wl1271_bss_info_changed_ap(wl, vif, bss_conf, changed);
	else
		wl1271_bss_info_changed_sta(wl, vif, bss_conf, changed);

	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
}

static void wl1271_op_get_current_rssi(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct station_info *sinfo)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int rssi = 0;
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 get current rssi");

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	if (wlvif->bss_type != BSS_TYPE_STA_BSS)
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl12xx_acx_sta_get_rssi(wl, wlvif, &rssi);
	if (ret < 0)
		goto out_sleep;

	sinfo->signal = (s8)rssi;

out_sleep:
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
}

static int wl12xx_op_set_rx_filters(struct ieee80211_hw *hw,
				    struct cfg80211_wowlan *wowlan)
{
	struct wl1271 *wl = hw->priv;
	int ret = 0;

	mutex_lock(&wl->mutex);

	wl1271_debug(DEBUG_MAC80211, "mac80211 set rx filters");

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_configure_wowlan(wl, wowlan);

	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl1271_op_conf_tx(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif, u16 queue,
			     const struct ieee80211_tx_queue_params *params)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	u8 ps_scheme;
	int ret = 0;

	mutex_lock(&wl->mutex);

	wl1271_debug(DEBUG_MAC80211, "mac80211 conf tx %d", queue);

	if (params->uapsd)
		ps_scheme = CONF_PS_SCHEME_UPSD_TRIGGER;
	else
		ps_scheme = CONF_PS_SCHEME_LEGACY;

	if (!test_bit(WLVIF_FLAG_INITIALIZED, &wlvif->flags))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	/*
	 * the txop is confed in units of 32us by the mac80211,
	 * we need us
	 */
	ret = wl1271_acx_ac_cfg(wl, wlvif, wl1271_tx_get_queue(queue),
				params->cw_min, params->cw_max,
				params->aifs, params->txop << 5);
	if (ret < 0)
		goto out_sleep;

	ret = wl1271_acx_tid_cfg(wl, wlvif, wl1271_tx_get_queue(queue),
				 CONF_CHANNEL_TYPE_EDCF,
				 wl1271_tx_get_queue(queue),
				 ps_scheme, CONF_ACK_POLICY_LEGACY,
				 0, 0);

out_sleep:
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static u64 wl1271_op_get_tsf(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif)
{

	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	u64 mactime = ULLONG_MAX;
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 get tsf");

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl12xx_acx_tsf_info(wl, wlvif, &mactime);
	if (ret < 0)
		goto out_sleep;

out_sleep:
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
	return mactime;
}

static int wl1271_op_get_survey(struct ieee80211_hw *hw, int idx,
				struct survey_info *survey)
{
	struct wl1271 *wl = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;

	if (idx != 0)
		return -ENOENT;

	survey->channel = conf->channel;
	survey->filled = SURVEY_INFO_NOISE_DBM;
	survey->noise = wl->noise;

	return 0;
}

static int wl1271_allocate_sta(struct wl1271 *wl,
			     struct wl12xx_vif *wlvif,
			     struct ieee80211_sta *sta)
{
	struct wl1271_station *wl_sta;
	int ret;


	if (wl->active_sta_count >= AP_MAX_STATIONS) {
		wl1271_warning("could not allocate HLID - too much stations");
		return -EBUSY;
	}

	wl_sta = (struct wl1271_station *)sta->drv_priv;
	ret = wl12xx_allocate_link(wl, wlvif, &wl_sta->hlid);
	if (ret < 0) {
		wl1271_warning("could not allocate HLID - too many links");
		return -EBUSY;
	}

	set_bit(wl_sta->hlid, wlvif->ap.sta_hlid_map);
	memcpy(wl->links[wl_sta->hlid].addr, sta->addr, ETH_ALEN);
	wl->active_sta_count++;
	return 0;
}

void wl1271_free_sta(struct wl1271 *wl, struct wl12xx_vif *wlvif, u8 hlid)
{
	if (!test_bit(hlid, wlvif->ap.sta_hlid_map))
		return;

	clear_bit(hlid, wlvif->ap.sta_hlid_map);
	memset(wl->links[hlid].addr, 0, ETH_ALEN);

	/* mac will send DELBA after recovery */
	if (!test_bit(WL1271_FLAG_RECOVERY_IN_PROGRESS, &wl->flags))
		wl->links[hlid].ba_bitmap = 0;

	__clear_bit(hlid, &wl->ap_ps_map);
	__clear_bit(hlid, (unsigned long *)&wl->ap_fw_ps_map);
	wl12xx_free_link(wl, wlvif, &hlid);
	wl->active_sta_count--;
}

void wl12xx_update_sta_state(struct wl1271 *wl,
			     struct ieee80211_sta *sta,
			     enum ieee80211_sta_state state)
{
	struct wl1271_station *wl_sta;
	u8 hlid;
	int ret;

	wl_sta = (struct wl1271_station *)sta->drv_priv;
	hlid = wl_sta->hlid;

	if (state == IEEE80211_STA_AUTHORIZED) {
		ret = wl12xx_cmd_set_peer_state(wl, hlid);
		if (ret < 0)
			return;

		ret = wl1271_acx_set_ht_capabilities(wl, &sta->ht_cap, true,
						     hlid);
		if (ret < 0)
			return;
	}
}

int wl1271_op_sta_add_locked(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct ieee80211_sta *sta)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	struct wl1271_station *wl_sta;
	int ret = 0;
	u8 hlid;
	struct ap_peers *new_peer;

	if (!test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags)) {
		new_peer = kzalloc(sizeof(struct ap_peers), GFP_KERNEL);
		memcpy(&new_peer->sta, sta, sizeof(struct ieee80211_sta));
		new_peer->hw = hw;
		new_peer->vif = vif;
		list_add(&new_peer->list, &wl->peers_list);
		wl1271_debug(DEBUG_AP, "add pending sta %pM to the list",
			     new_peer->sta.addr);
		goto out;
	}

	ret = wl1271_allocate_sta(wl, wlvif, sta);
	if (ret < 0)
		goto out;

	wl_sta = (struct wl1271_station *)sta->drv_priv;
	hlid = wl_sta->hlid;

	ret = wl12xx_cmd_add_peer(wl, wlvif, sta, hlid);
	if (ret < 0)
		wl1271_free_sta(wl, wlvif, hlid);

out:
	return ret;
}

static int wl1271_op_sta_add(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct ieee80211_sta *sta)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret = 0;

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	if (wlvif->bss_type != BSS_TYPE_AP_BSS)
		goto out;

	wl1271_debug(DEBUG_MAC80211, "mac80211 add sta %d", (int)sta->aid);

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_op_sta_add_locked(hw, vif, sta);

	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
	return ret;
}

static int wl1271_op_sta_remove(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	struct wl1271_station *wl_sta;
	int ret = 0, id;

	mutex_lock(&wl->mutex);

	if (!list_empty(&wl->peers_list) &&
	    !test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags)) {
		struct ap_peers *peer, *next;
		list_for_each_entry_safe(peer, next, &wl->peers_list, list) {
			if (compare_ether_addr(sta->addr, peer->sta.addr))
				continue;

			wl1271_debug(DEBUG_AP, "remove pending sta %pM from the"
				     " list", peer->sta.addr);
			list_del(&peer->list);
			kfree(peer);
		}
		goto out;
	}

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	if (wlvif->bss_type != BSS_TYPE_AP_BSS)
		goto out;

	wl1271_debug(DEBUG_MAC80211, "mac80211 remove sta %d", (int)sta->aid);

	wl_sta = (struct wl1271_station *)sta->drv_priv;
	id = wl_sta->hlid;
	if (WARN_ON(!test_bit(id, wlvif->ap.sta_hlid_map)))
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl12xx_cmd_remove_peer(wl, wl_sta->hlid);
	if (ret < 0)
		goto out_sleep;

	wl1271_free_sta(wl, wlvif, wl_sta->hlid);

out_sleep:
	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
	return ret;
}

void wl12xx_op_sta_state(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta,
			 enum ieee80211_sta_state state)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret;

	if (!test_bit(WLVIF_FLAG_AP_STARTED, &wlvif->flags))
		return;

	wl1271_debug(DEBUG_MAC80211, "mac80211 sta %d state=%d",
		     sta->aid, state);

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	if (wlvif->bss_type != BSS_TYPE_AP_BSS)
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl12xx_update_sta_state(wl, sta, state);

	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
}

static int wl1271_op_ampdu_action(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  enum ieee80211_ampdu_mlme_action action,
				  struct ieee80211_sta *sta, u16 tid, u16 *ssn,
				  u8 buf_size)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	int ret;
	u8 hlid, *ba_bitmap;

	wl1271_debug(DEBUG_MAC80211, "mac80211 ampdu action %d tid %d", action,
		     tid);

	/* sanity check - the fields in FW are only 8bits wide */
	if (WARN_ON(tid > 0xFF))
		return -ENOTSUPP;

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF)) {
		ret = -EAGAIN;
		goto out;
	}

	if (wlvif->bss_type == BSS_TYPE_STA_BSS) {
		hlid = wlvif->sta.hlid;
		ba_bitmap = &wlvif->sta.ba_rx_bitmap;
	} else if (wlvif->bss_type == BSS_TYPE_AP_BSS) {
		struct wl1271_station *wl_sta;

		wl_sta = (struct wl1271_station *)sta->drv_priv;
		hlid = wl_sta->hlid;
		ba_bitmap = &wl->links[hlid].ba_bitmap;
	} else {
		ret = -EINVAL;
		goto out;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1271_debug(DEBUG_MAC80211, "mac80211 ampdu: Rx tid %d action %d",
		     tid, action);

	switch (action) {
	case IEEE80211_AMPDU_RX_START:
		if (!wlvif->ba_support || !wlvif->ba_allowed) {
			ret = -ENOTSUPP;
			break;
		}

		if (wl->ba_rx_session_count >= RX_BA_MAX_SESSIONS) {
			ret = -EBUSY;
			wl1271_error("exceeded max RX BA sessions");
			break;
		}

		if (*ba_bitmap & BIT(tid)) {
			ret = -EINVAL;
			wl1271_error("cannot enable RX BA session on active "
				     "tid: %d", tid);
			break;
		}

		ret = wl12xx_acx_set_ba_receiver_session(wl, tid, *ssn, true,
							 hlid);
		if (!ret) {
			*ba_bitmap |= BIT(tid);
			wl->ba_rx_session_count++;
		}
		break;

	case IEEE80211_AMPDU_RX_STOP:
		if (!(*ba_bitmap & BIT(tid))) {
			ret = -EINVAL;
			wl1271_error("no active RX BA session on tid: %d",
				     tid);
			break;
		}

		ret = wl12xx_acx_set_ba_receiver_session(wl, tid, 0, false,
							 hlid);
		if (!ret) {
			*ba_bitmap &= ~BIT(tid);
			if (wl->ba_rx_session_count > 0)
				wl->ba_rx_session_count--;
		}
		break;

	/*
	 * The BA initiator session management in FW independently.
	 * Falling break here on purpose for all TX APDU commands.
	 */
	case IEEE80211_AMPDU_TX_START:
	case IEEE80211_AMPDU_TX_STOP:
	case IEEE80211_AMPDU_TX_OPERATIONAL:
		ret = -EINVAL;
		break;

	default:
		wl1271_error("Incorrect ampdu action id=%x\n", action);
		ret = -EINVAL;
	}

	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl12xx_set_bitrate_mask(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   const struct cfg80211_bitrate_mask *mask)
{
	struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
	struct wl1271 *wl = hw->priv;
	int i, ret = 0;

	wl1271_debug(DEBUG_MAC80211, "mac80211 set_bitrate_mask 0x%x 0x%x",
		mask->control[NL80211_BAND_2GHZ].legacy,
		mask->control[NL80211_BAND_5GHZ].legacy);

	mutex_lock(&wl->mutex);

	for (i = 0; i < IEEE80211_NUM_BANDS; i++)
		wlvif->bitrate_masks[i] =
			wl1271_tx_enabled_rates_get(wl,
						    mask->control[i].legacy,
						    i);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	if (wlvif->bss_type == BSS_TYPE_STA_BSS &&
	    !test_bit(WLVIF_FLAG_STA_ASSOCIATED, &wlvif->flags)) {

		ret = wl1271_ps_elp_wakeup(wl);
		if (ret < 0)
			goto out;

		wl1271_set_band_rate(wl, wlvif);
		wlvif->basic_rate =
			wl1271_tx_min_rate_get(wl, wlvif->basic_rate_set);
		ret = wl1271_acx_sta_rate_policies(wl, wlvif);

		wl1271_ps_elp_sleep(wl);
	}
out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static void wl12xx_op_channel_switch(struct ieee80211_hw *hw,
				     struct ieee80211_channel_switch *ch_switch)
{
	struct wl1271 *wl = hw->priv;
	struct wl12xx_vif *wlvif;
	int ret;

	wl1271_debug(DEBUG_MAC80211, "mac80211 channel switch");

	wl1271_tx_flush(wl);

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF)) {
		wl12xx_for_each_wlvif_sta(wl, wlvif) {
			struct ieee80211_vif *vif = wl12xx_wlvif_to_vif(wlvif);
			ieee80211_chswitch_done(vif, false);
		}
		goto out;
	}

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	/* TODO: change mac80211 to pass vif as param */
	wl12xx_for_each_wlvif_sta(wl, wlvif) {
		ret = wl12xx_cmd_channel_switch(wl, wlvif, ch_switch);

		if (!ret)
			set_bit(WLVIF_FLAG_CS_PROGRESS, &wlvif->flags);
	}

	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
}

static bool wl1271_tx_frames_pending(struct ieee80211_hw *hw)
{
	struct wl1271 *wl = hw->priv;
	bool ret = false;

	mutex_lock(&wl->mutex);

	if (unlikely(wl->state == WL1271_STATE_OFF))
		goto out;

	/* packets are considered pending if in the TX queue or the FW */
	ret = (wl1271_tx_total_queue_count(wl) > 0) || (wl->tx_frames_cnt > 0);
out:
	mutex_unlock(&wl->mutex);

	return ret;
}

/* can't be const, mac80211 writes to this */
static struct ieee80211_rate wl1271_rates[] = {
	{ .bitrate = 10,
	  .hw_value = CONF_HW_BIT_RATE_1MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_1MBPS, },
	{ .bitrate = 20,
	  .hw_value = CONF_HW_BIT_RATE_2MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_2MBPS,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55,
	  .hw_value = CONF_HW_BIT_RATE_5_5MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_5_5MBPS,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110,
	  .hw_value = CONF_HW_BIT_RATE_11MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_11MBPS,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 60,
	  .hw_value = CONF_HW_BIT_RATE_6MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_6MBPS, },
	{ .bitrate = 90,
	  .hw_value = CONF_HW_BIT_RATE_9MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_9MBPS, },
	{ .bitrate = 120,
	  .hw_value = CONF_HW_BIT_RATE_12MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_12MBPS, },
	{ .bitrate = 180,
	  .hw_value = CONF_HW_BIT_RATE_18MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_18MBPS, },
	{ .bitrate = 240,
	  .hw_value = CONF_HW_BIT_RATE_24MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_24MBPS, },
	{ .bitrate = 360,
	 .hw_value = CONF_HW_BIT_RATE_36MBPS,
	 .hw_value_short = CONF_HW_BIT_RATE_36MBPS, },
	{ .bitrate = 480,
	  .hw_value = CONF_HW_BIT_RATE_48MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_48MBPS, },
	{ .bitrate = 540,
	  .hw_value = CONF_HW_BIT_RATE_54MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_54MBPS, },
};

/* can't be const, mac80211 writes to this */
static struct ieee80211_channel wl1271_channels[] = {
	{ .hw_value = 1, .center_freq = 2412, .max_power = 25 },
	{ .hw_value = 2, .center_freq = 2417, .max_power = 25 },
	{ .hw_value = 3, .center_freq = 2422, .max_power = 25 },
	{ .hw_value = 4, .center_freq = 2427, .max_power = 25 },
	{ .hw_value = 5, .center_freq = 2432, .max_power = 25 },
	{ .hw_value = 6, .center_freq = 2437, .max_power = 25 },
	{ .hw_value = 7, .center_freq = 2442, .max_power = 25 },
	{ .hw_value = 8, .center_freq = 2447, .max_power = 25 },
	{ .hw_value = 9, .center_freq = 2452, .max_power = 25 },
	{ .hw_value = 10, .center_freq = 2457, .max_power = 25 },
	{ .hw_value = 11, .center_freq = 2462, .max_power = 25 },
	{ .hw_value = 12, .center_freq = 2467, .max_power = 25 },
	{ .hw_value = 13, .center_freq = 2472, .max_power = 25 },
	{ .hw_value = 14, .center_freq = 2484, .max_power = 25 },
};

/* mapping to indexes for wl1271_rates */
static const u8 wl1271_rate_to_idx_2ghz[] = {
	/* MCS rates are used only with 11n */
	7,                            /* CONF_HW_RXTX_RATE_MCS7_SGI */
	7,                            /* CONF_HW_RXTX_RATE_MCS7 */
	6,                            /* CONF_HW_RXTX_RATE_MCS6 */
	5,                            /* CONF_HW_RXTX_RATE_MCS5 */
	4,                            /* CONF_HW_RXTX_RATE_MCS4 */
	3,                            /* CONF_HW_RXTX_RATE_MCS3 */
	2,                            /* CONF_HW_RXTX_RATE_MCS2 */
	1,                            /* CONF_HW_RXTX_RATE_MCS1 */
	0,                            /* CONF_HW_RXTX_RATE_MCS0 */

	11,                            /* CONF_HW_RXTX_RATE_54   */
	10,                            /* CONF_HW_RXTX_RATE_48   */
	9,                             /* CONF_HW_RXTX_RATE_36   */
	8,                             /* CONF_HW_RXTX_RATE_24   */

	/* TI-specific rate */
	CONF_HW_RXTX_RATE_UNSUPPORTED, /* CONF_HW_RXTX_RATE_22   */

	7,                             /* CONF_HW_RXTX_RATE_18   */
	6,                             /* CONF_HW_RXTX_RATE_12   */
	3,                             /* CONF_HW_RXTX_RATE_11   */
	5,                             /* CONF_HW_RXTX_RATE_9    */
	4,                             /* CONF_HW_RXTX_RATE_6    */
	2,                             /* CONF_HW_RXTX_RATE_5_5  */
	1,                             /* CONF_HW_RXTX_RATE_2    */
	0                              /* CONF_HW_RXTX_RATE_1    */
};

/* 11n STA capabilities */
#define HW_RX_HIGHEST_RATE	72

#define WL12XX_HT_CAP { \
	.cap = IEEE80211_HT_CAP_GRN_FLD | IEEE80211_HT_CAP_SGI_20 | \
	       (1 << IEEE80211_HT_CAP_RX_STBC_SHIFT), \
	.ht_supported = true, \
	.ampdu_factor = IEEE80211_HT_MAX_AMPDU_8K, \
	.ampdu_density = IEEE80211_HT_MPDU_DENSITY_8, \
	.mcs = { \
		.rx_mask = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, }, \
		.rx_highest = cpu_to_le16(HW_RX_HIGHEST_RATE), \
		.tx_params = IEEE80211_HT_MCS_TX_DEFINED, \
		}, \
}

/* can't be const, mac80211 writes to this */
static struct ieee80211_supported_band wl1271_band_2ghz = {
	.channels = wl1271_channels,
	.n_channels = ARRAY_SIZE(wl1271_channels),
	.bitrates = wl1271_rates,
	.n_bitrates = ARRAY_SIZE(wl1271_rates),
	.ht_cap	= WL12XX_HT_CAP,
};

/* 5 GHz data rates for WL1273 */
static struct ieee80211_rate wl1271_rates_5ghz[] = {
	{ .bitrate = 60,
	  .hw_value = CONF_HW_BIT_RATE_6MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_6MBPS, },
	{ .bitrate = 90,
	  .hw_value = CONF_HW_BIT_RATE_9MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_9MBPS, },
	{ .bitrate = 120,
	  .hw_value = CONF_HW_BIT_RATE_12MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_12MBPS, },
	{ .bitrate = 180,
	  .hw_value = CONF_HW_BIT_RATE_18MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_18MBPS, },
	{ .bitrate = 240,
	  .hw_value = CONF_HW_BIT_RATE_24MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_24MBPS, },
	{ .bitrate = 360,
	 .hw_value = CONF_HW_BIT_RATE_36MBPS,
	 .hw_value_short = CONF_HW_BIT_RATE_36MBPS, },
	{ .bitrate = 480,
	  .hw_value = CONF_HW_BIT_RATE_48MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_48MBPS, },
	{ .bitrate = 540,
	  .hw_value = CONF_HW_BIT_RATE_54MBPS,
	  .hw_value_short = CONF_HW_BIT_RATE_54MBPS, },
};

/* 5 GHz band channels for WL1273 */
static struct ieee80211_channel wl1271_channels_5ghz[] = {
	{ .hw_value = 7, .center_freq = 5035, .max_power = 25 },
	{ .hw_value = 8, .center_freq = 5040, .max_power = 25 },
	{ .hw_value = 9, .center_freq = 5045, .max_power = 25 },
	{ .hw_value = 11, .center_freq = 5055, .max_power = 25 },
	{ .hw_value = 12, .center_freq = 5060, .max_power = 25 },
	{ .hw_value = 16, .center_freq = 5080, .max_power = 25 },
	{ .hw_value = 34, .center_freq = 5170, .max_power = 25 },
	{ .hw_value = 36, .center_freq = 5180, .max_power = 25 },
	{ .hw_value = 38, .center_freq = 5190, .max_power = 25 },
	{ .hw_value = 40, .center_freq = 5200, .max_power = 25 },
	{ .hw_value = 42, .center_freq = 5210, .max_power = 25 },
	{ .hw_value = 44, .center_freq = 5220, .max_power = 25 },
	{ .hw_value = 46, .center_freq = 5230, .max_power = 25 },
	{ .hw_value = 48, .center_freq = 5240, .max_power = 25 },
	{ .hw_value = 52, .center_freq = 5260, .max_power = 25 },
	{ .hw_value = 56, .center_freq = 5280, .max_power = 25 },
	{ .hw_value = 60, .center_freq = 5300, .max_power = 25 },
	{ .hw_value = 64, .center_freq = 5320, .max_power = 25 },
	{ .hw_value = 100, .center_freq = 5500, .max_power = 25 },
	{ .hw_value = 104, .center_freq = 5520, .max_power = 25 },
	{ .hw_value = 108, .center_freq = 5540, .max_power = 25 },
	{ .hw_value = 112, .center_freq = 5560, .max_power = 25 },
	{ .hw_value = 116, .center_freq = 5580, .max_power = 25 },
	{ .hw_value = 120, .center_freq = 5600, .max_power = 25 },
	{ .hw_value = 124, .center_freq = 5620, .max_power = 25 },
	{ .hw_value = 128, .center_freq = 5640, .max_power = 25 },
	{ .hw_value = 132, .center_freq = 5660, .max_power = 25 },
	{ .hw_value = 136, .center_freq = 5680, .max_power = 25 },
	{ .hw_value = 140, .center_freq = 5700, .max_power = 25 },
	{ .hw_value = 149, .center_freq = 5745, .max_power = 25 },
	{ .hw_value = 153, .center_freq = 5765, .max_power = 25 },
	{ .hw_value = 157, .center_freq = 5785, .max_power = 25 },
	{ .hw_value = 161, .center_freq = 5805, .max_power = 25 },
	{ .hw_value = 165, .center_freq = 5825, .max_power = 25 },
};

/* mapping to indexes for wl1271_rates_5ghz */
static const u8 wl1271_rate_to_idx_5ghz[] = {
	/* MCS rates are used only with 11n */
	7,                            /* CONF_HW_RXTX_RATE_MCS7_SGI */
	7,                            /* CONF_HW_RXTX_RATE_MCS7 */
	6,                            /* CONF_HW_RXTX_RATE_MCS6 */
	5,                            /* CONF_HW_RXTX_RATE_MCS5 */
	4,                            /* CONF_HW_RXTX_RATE_MCS4 */
	3,                            /* CONF_HW_RXTX_RATE_MCS3 */
	2,                            /* CONF_HW_RXTX_RATE_MCS2 */
	1,                            /* CONF_HW_RXTX_RATE_MCS1 */
	0,                            /* CONF_HW_RXTX_RATE_MCS0 */

	7,                             /* CONF_HW_RXTX_RATE_54   */
	6,                             /* CONF_HW_RXTX_RATE_48   */
	5,                             /* CONF_HW_RXTX_RATE_36   */
	4,                             /* CONF_HW_RXTX_RATE_24   */

	/* TI-specific rate */
	CONF_HW_RXTX_RATE_UNSUPPORTED, /* CONF_HW_RXTX_RATE_22   */

	3,                             /* CONF_HW_RXTX_RATE_18   */
	2,                             /* CONF_HW_RXTX_RATE_12   */
	CONF_HW_RXTX_RATE_UNSUPPORTED, /* CONF_HW_RXTX_RATE_11   */
	1,                             /* CONF_HW_RXTX_RATE_9    */
	0,                             /* CONF_HW_RXTX_RATE_6    */
	CONF_HW_RXTX_RATE_UNSUPPORTED, /* CONF_HW_RXTX_RATE_5_5  */
	CONF_HW_RXTX_RATE_UNSUPPORTED, /* CONF_HW_RXTX_RATE_2    */
	CONF_HW_RXTX_RATE_UNSUPPORTED  /* CONF_HW_RXTX_RATE_1    */
};

static struct ieee80211_supported_band wl1271_band_5ghz = {
	.channels = wl1271_channels_5ghz,
	.n_channels = ARRAY_SIZE(wl1271_channels_5ghz),
	.bitrates = wl1271_rates_5ghz,
	.n_bitrates = ARRAY_SIZE(wl1271_rates_5ghz),
	.ht_cap	= WL12XX_HT_CAP,
};

static const u8 *wl1271_band_rate_to_idx[] = {
	[IEEE80211_BAND_2GHZ] = wl1271_rate_to_idx_2ghz,
	[IEEE80211_BAND_5GHZ] = wl1271_rate_to_idx_5ghz
};

static const struct ieee80211_ops wl1271_ops = {
	.start = wl1271_op_start,
	.stop = wl1271_op_stop,
	.add_interface = wl1271_op_add_interface,
	.remove_interface = wl1271_op_remove_interface,
	.change_interface = wl12xx_op_change_interface,
#ifdef CONFIG_PM
	.suspend = wl1271_op_suspend,
	.resume = wl1271_op_resume,
#endif
	.config = wl1271_op_config,
	.prepare_multicast = wl1271_op_prepare_multicast,
	.configure_filter = wl1271_op_configure_filter,
	.tx = wl1271_op_tx,
	.set_key = wl1271_op_set_key,
	.hw_scan = wl1271_op_hw_scan,
	.cancel_hw_scan = wl1271_op_cancel_hw_scan,
	.sched_scan_start = wl1271_op_sched_scan_start,
	.sched_scan_stop = wl1271_op_sched_scan_stop,
	.bss_info_changed = wl1271_op_bss_info_changed,
	.set_frag_threshold = wl1271_op_set_frag_threshold,
	.set_rts_threshold = wl1271_op_set_rts_threshold,
	.conf_tx = wl1271_op_conf_tx,
	.get_tsf = wl1271_op_get_tsf,
	.get_survey = wl1271_op_get_survey,
	.sta_add = wl1271_op_sta_add,
	.sta_remove = wl1271_op_sta_remove,
	.sta_state = wl12xx_op_sta_state,
	.ampdu_action = wl1271_op_ampdu_action,
	.tx_frames_pending = wl1271_tx_frames_pending,
	.set_bitrate_mask = wl12xx_set_bitrate_mask,
	.channel_switch = wl12xx_op_channel_switch,
	.set_default_key_idx = wl1271_op_set_default_key_idx,
	.get_current_rssi = wl1271_op_get_current_rssi,
	.set_rx_filters = wl12xx_op_set_rx_filters,
	CFG80211_TESTMODE_CMD(wl1271_tm_cmd)
};


u8 wl1271_rate_to_idx(int rate, enum ieee80211_band band)
{
	u8 idx;

	BUG_ON(band >= sizeof(wl1271_band_rate_to_idx)/sizeof(u8 *));

	if (unlikely(rate >= CONF_HW_RXTX_RATE_MAX)) {
		wl1271_error("Illegal RX rate from HW: %d", rate);
		return 0;
	}

	idx = wl1271_band_rate_to_idx[band][rate];
	if (unlikely(idx == CONF_HW_RXTX_RATE_UNSUPPORTED)) {
		wl1271_error("Unsupported RX rate from HW: %d", rate);
		return 0;
	}

	return idx;
}

static ssize_t wl1271_sysfs_show_bt_coex_state(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct wl1271 *wl = dev_get_drvdata(dev);
	ssize_t len;

	len = PAGE_SIZE;

	mutex_lock(&wl->mutex);
	len = snprintf(buf, len, "%d\n\n0 - off\n1 - on\n",
		       wl->sg_enabled);
	mutex_unlock(&wl->mutex);

	return len;

}

static ssize_t wl1271_sysfs_store_bt_coex_state(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct wl1271 *wl = dev_get_drvdata(dev);
	unsigned long res;
	int ret;

	ret = kstrtoul(buf, 10, &res);
	if (ret < 0) {
		wl1271_warning("incorrect value written to bt_coex_mode");
		return count;
	}

	mutex_lock(&wl->mutex);

	res = !!res;

	if (res == wl->sg_enabled)
		goto out;

	wl->sg_enabled = res;

	if (wl->state == WL1271_STATE_OFF)
		goto out;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1271_acx_sg_enable(wl, wl->sg_enabled);
	wl1271_ps_elp_sleep(wl);

 out:
	mutex_unlock(&wl->mutex);
	return count;
}

static DEVICE_ATTR(bt_coex_state, S_IRUGO | S_IWUSR,
		   wl1271_sysfs_show_bt_coex_state,
		   wl1271_sysfs_store_bt_coex_state);

static ssize_t wl1271_sysfs_show_hw_pg_ver(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct wl1271 *wl = dev_get_drvdata(dev);
	ssize_t len;

	len = PAGE_SIZE;

	mutex_lock(&wl->mutex);
	if (wl->hw_pg_ver >= 0)
		len = snprintf(buf, len, "%d\n", wl->hw_pg_ver);
	else
		len = snprintf(buf, len, "n/a\n");
	mutex_unlock(&wl->mutex);

	return len;
}

static DEVICE_ATTR(hw_pg_ver, S_IRUGO,
		   wl1271_sysfs_show_hw_pg_ver, NULL);

static ssize_t wl1271_sysfs_read_fwlog(struct file *filp, struct kobject *kobj,
				       struct bin_attribute *bin_attr,
				       char *buffer, loff_t pos, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct wl1271 *wl = dev_get_drvdata(dev);
	ssize_t len;
	int ret;

	ret = mutex_lock_interruptible(&wl->mutex);
	if (ret < 0)
		return -ERESTARTSYS;

	/* Let only one thread read the log at a time, blocking others */
	while (wl->fwlog_size == 0) {
		DEFINE_WAIT(wait);

		prepare_to_wait_exclusive(&wl->fwlog_waitq,
					  &wait,
					  TASK_INTERRUPTIBLE);

		if (wl->fwlog_size != 0) {
			finish_wait(&wl->fwlog_waitq, &wait);
			break;
		}

		mutex_unlock(&wl->mutex);

		schedule();
		finish_wait(&wl->fwlog_waitq, &wait);

		if (signal_pending(current))
			return -ERESTARTSYS;

		ret = mutex_lock_interruptible(&wl->mutex);
		if (ret < 0)
			return -ERESTARTSYS;
	}

	/* Check if the fwlog is still valid */
	if (wl->fwlog_size < 0) {
		mutex_unlock(&wl->mutex);
		return 0;
	}

	/* Seeking is not supported - old logs are not kept. Disregard pos. */
	len = min(count, (size_t)wl->fwlog_size);
	wl->fwlog_size -= len;
	memcpy(buffer, wl->fwlog, len);

	/* Make room for new messages */
	memmove(wl->fwlog, wl->fwlog + len, wl->fwlog_size);

	mutex_unlock(&wl->mutex);

	return len;
}

static struct bin_attribute fwlog_attr = {
	.attr = {.name = "fwlog", .mode = S_IRUSR},
	.read = wl1271_sysfs_read_fwlog,
};

static bool wl12xx_mac_in_fuse(struct wl1271 *wl)
{
	bool supported = false;
	u8 major, minor;

	if (wl->chip.id == CHIP_ID_1283_PG20) {
		major = WL128X_PG_GET_MAJOR(wl->hw_pg_ver);
		minor = WL128X_PG_GET_MINOR(wl->hw_pg_ver);

		/* in wl128x we have the MAC address if the PG is >= (2, 1) */
		if (major > 2 || (major == 2 && minor >= 1))
			supported = true;
	} else {
		major = WL127X_PG_GET_MAJOR(wl->hw_pg_ver);
		minor = WL127X_PG_GET_MINOR(wl->hw_pg_ver);

		/* in wl127x we have the MAC address if the PG is >= (3, 1) */
		if (major == 3 && minor >= 1)
			supported = true;
	}

	wl1271_debug(DEBUG_PROBE,
		     "PG Ver major = %d minor = %d, MAC %s present",
		     major, minor, supported ? "is" : "is not");

	return supported;
}

static void wl12xx_derive_mac_addresses(struct wl1271 *wl,
					u32 oui, u32 nic, int n)
{
	int i;

	wl1271_debug(DEBUG_PROBE, "base address: oui %06x nic %06x, n %d",
		     oui, nic, n);

	if (nic + n - 1 > 0xffffff)
		wl1271_warning("NIC part of the MAC address wraps around!");

	for (i = 0; i < n; i++) {
		wl->addresses[i].addr[0] = (u8)(oui >> 16);
		wl->addresses[i].addr[1] = (u8)(oui >> 8);
		wl->addresses[i].addr[2] = (u8) oui;
		wl->addresses[i].addr[3] = (u8)(nic >> 16);
		wl->addresses[i].addr[4] = (u8)(nic >> 8);
		wl->addresses[i].addr[5] = (u8) nic;
		nic++;
	}

	wl->hw->wiphy->n_addresses = n;
	wl->hw->wiphy->addresses = wl->addresses;
}

int wl12xx_init_pll_clock(struct wl1271 *wl, int *selected_clock)
{
	int ret;

	if (wl->chip.id == CHIP_ID_1283_PG20) {
		ret = wl128x_boot_clk(wl, selected_clock);
		if (ret < 0)
			return ret;
	} else {
		ret = wl127x_boot_clk(wl);
		if (ret < 0)
			return ret;
	}

	/* Continue the ELP wake up sequence */
	ret = wl1271_write32(wl, WELP_ARM_COMMAND, WELP_ARM_COMMAND_VAL);
	if (ret < 0)
		return ret;

	udelay(500);
	return ret;
}

static int wl12xx_get_fuse_mac(struct wl1271 *wl)
{
	u32 mac1, mac2;
	int ret;
	int selected_clock = -1;

	ret = wl12xx_init_pll_clock(wl, &selected_clock);
	if (ret < 0)
		goto out;

	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_DRPW]);
	if (ret < 0)
		goto out;

	ret = wl1271_read32(wl, WL12XX_REG_FUSE_BD_ADDR_1, &mac1);
	if (ret < 0)
		goto out;

	ret = wl1271_read32(wl, WL12XX_REG_FUSE_BD_ADDR_2, &mac2);
	if (ret < 0)
		goto out;

	/* these are the two parts of the BD_ADDR */
	wl->fuse_oui_addr = ((mac2 & 0xffff) << 8) +
		((mac1 & 0xff000000) >> 24);
	wl->fuse_nic_addr = mac1 & 0xffffff;

	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_DOWN]);

out:
	return ret;
}

static int wl12xx_get_hw_info(struct wl1271 *wl)
{
	int ret;
	u16 die_info;

	ret = wl12xx_set_power_on(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_read32(wl, CHIP_ID_B, &wl->chip.id);
	if (ret < 0)
		goto out;

	if (wl->chip.id == CHIP_ID_1283_PG20)
		ret = wl1271_top_reg_read(wl, WL128X_REG_FUSE_DATA_2_1,
					  &die_info);
	else
		ret = wl1271_top_reg_read(wl, WL127X_REG_FUSE_DATA_2_1,
					  &die_info);
	if (ret < 0)
		goto out;

	wl->hw_pg_ver = (s8) (die_info & PG_VER_MASK) >> PG_VER_OFFSET;

	if (!wl12xx_mac_in_fuse(wl)) {
		wl->fuse_oui_addr = 0;
		wl->fuse_nic_addr = 0;
	} else {
		ret = wl12xx_get_fuse_mac(wl);
		if (ret < 0)
			goto out;
	}

out:
	wl1271_power_off(wl);
	return ret;
}

static int wl1271_register_hw(struct wl1271 *wl)
{
	int ret;
	u32 oui_addr = 0, nic_addr = 0;

	if (wl->mac80211_registered)
		return 0;

	ret = wl12xx_get_hw_info(wl);
	if (ret < 0) {
		wl1271_error("couldn't get hw info");
		goto out;
	}

	ret = wl1271_fetch_nvs(wl);
	if (ret == 0) {
		/* NOTE: The wl->nvs->nvs element must be first, in
		 * order to simplify the casting, we assume it is at
		 * the beginning of the wl->nvs structure.
		 */
		u8 *nvs_ptr = (u8 *)wl->nvs;

		oui_addr =
			(nvs_ptr[11] << 16) + (nvs_ptr[10] << 8) + nvs_ptr[6];
		nic_addr =
			(nvs_ptr[5] << 16) + (nvs_ptr[4] << 8) + nvs_ptr[3];
	}

	/* if the MAC address is zeroed in the NVS derive from fuse */
	if (oui_addr == 0 && nic_addr == 0) {
		oui_addr = wl->fuse_oui_addr;
		/* fuse has the BD_ADDR, the WLAN addresses are the next two */
		nic_addr = wl->fuse_nic_addr + 1;
	}

	wl12xx_derive_mac_addresses(wl, oui_addr, nic_addr, 2);

	ret = ieee80211_register_hw(wl->hw);
	if (ret < 0) {
		wl1271_error("unable to register mac80211 hw: %d", ret);
		goto out;
	}

	wl->mac80211_registered = true;

	wl1271_debugfs_init(wl);

	register_netdevice_notifier(&wl1271_dev_notifier);

	wl1271_notice("loaded");

out:
	return ret;
}

static void wl1271_unregister_hw(struct wl1271 *wl)
{
	if (wl->state == WL1271_STATE_PLT)
		wl1271_plt_stop(wl);

	unregister_netdevice_notifier(&wl1271_dev_notifier);
	ieee80211_unregister_hw(wl->hw);
	wl->mac80211_registered = false;

}

static int wl1271_init_ieee80211(struct wl1271 *wl)
{
	static const u32 cipher_suites[] = {
		WLAN_CIPHER_SUITE_WEP40,
		WLAN_CIPHER_SUITE_WEP104,
		WLAN_CIPHER_SUITE_TKIP,
		WLAN_CIPHER_SUITE_CCMP,
		WL1271_CIPHER_SUITE_GEM,
	};

	/* The tx descriptor buffer and the TKIP space. */
	wl->hw->extra_tx_headroom = WL1271_EXTRA_SPACE_TKIP +
		sizeof(struct wl1271_tx_hw_descr);

	/* unit us */
	/* FIXME: find a proper value */
	wl->hw->channel_change_time = 10000;
	wl->hw->max_listen_interval = wl->conf.conn.max_listen_interval;

	wl->hw->flags = IEEE80211_HW_SIGNAL_DBM |
		IEEE80211_HW_BEACON_FILTER |
		IEEE80211_HW_SUPPORTS_PS |
		IEEE80211_HW_SUPPORTS_DYNAMIC_PS |
		IEEE80211_HW_SUPPORTS_UAPSD |
		IEEE80211_HW_HAS_RATE_CONTROL |
		IEEE80211_HW_CONNECTION_MONITOR |
		IEEE80211_HW_SUPPORTS_CQM_RSSI |
		IEEE80211_HW_REPORTS_TX_ACK_STATUS |
		IEEE80211_HW_SPECTRUM_MGMT |
		IEEE80211_HW_AP_LINK_PS |
		IEEE80211_HW_AMPDU_AGGREGATION |
		IEEE80211_HW_TX_AMPDU_SETUP_IN_HW |
		IEEE80211_HW_SCAN_WHILE_IDLE |
		IEEE80211_HW_SUPPORTS_RX_FILTERS;

	wl->hw->wiphy->cipher_suites = cipher_suites;
	wl->hw->wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);

	wl->hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
		BIT(NL80211_IFTYPE_ADHOC) | BIT(NL80211_IFTYPE_AP) |
		BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO);
	wl->hw->wiphy->features |= NL80211_FEATURE_SCHED_SCAN_INTERVALS;
	wl->hw->wiphy->max_scan_ssids = 1;
	wl->hw->wiphy->max_sched_scan_ssids = 16;
	wl->hw->wiphy->max_match_sets = 16;
	/*
	 * Maximum length of elements in scanning probe request templates
	 * should be the maximum length possible for a template, without
	 * the IEEE80211 header of the template
	 */
	wl->hw->wiphy->max_scan_ie_len = WL1271_CMD_TEMPL_MAX_SIZE -
			sizeof(struct ieee80211_header);

	wl->hw->wiphy->max_sched_scan_ie_len = WL1271_CMD_TEMPL_MAX_SIZE -
		sizeof(struct ieee80211_header);

	wl->hw->wiphy->flags |= WIPHY_FLAG_AP_UAPSD;

	/* make sure all our channels fit in the scanned_ch bitmask */
	BUILD_BUG_ON(ARRAY_SIZE(wl1271_channels) +
		     ARRAY_SIZE(wl1271_channels_5ghz) >
		     WL1271_MAX_CHANNELS);
	/*
	 * We keep local copies of the band structs because we need to
	 * modify them on a per-device basis.
	 */
	memcpy(&wl->bands[IEEE80211_BAND_2GHZ], &wl1271_band_2ghz,
	       sizeof(wl1271_band_2ghz));
	memcpy(&wl->bands[IEEE80211_BAND_5GHZ], &wl1271_band_5ghz,
	       sizeof(wl1271_band_5ghz));

	wl->hw->wiphy->bands[IEEE80211_BAND_2GHZ] =
		&wl->bands[IEEE80211_BAND_2GHZ];
	wl->hw->wiphy->bands[IEEE80211_BAND_5GHZ] =
		&wl->bands[IEEE80211_BAND_5GHZ];

	wl->hw->queues = 4;
	wl->hw->max_rates = 1;

	wl->hw->wiphy->reg_notifier = wl1271_reg_notify;

	/* the FW answers probe-requests in AP-mode */
	wl->hw->wiphy->flags |= WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD;
	wl->hw->wiphy->probe_resp_offload =
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2 |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P;

	SET_IEEE80211_DEV(wl->hw, wl->dev);

	wl->hw->sta_data_size = sizeof(struct wl1271_station);
	wl->hw->vif_data_size = sizeof(struct wl12xx_vif);

	wl->hw->max_rx_aggregation_subframes = 8;

	return 0;
}

#define WL1271_DEFAULT_CHANNEL 0

static struct ieee80211_hw *wl1271_alloc_hw(void)
{
	struct ieee80211_hw *hw;
	struct wl1271 *wl;
	int i, j, ret;
	unsigned int order;

	BUILD_BUG_ON(AP_MAX_STATIONS > WL12XX_MAX_LINKS);

	hw = ieee80211_alloc_hw(sizeof(*wl), &wl1271_ops);
	if (!hw) {
		wl1271_error("could not alloc ieee80211_hw");
		ret = -ENOMEM;
		goto err_hw_alloc;
	}

	wl = hw->priv;
	memset(wl, 0, sizeof(*wl));

	INIT_LIST_HEAD(&wl->list);
	INIT_LIST_HEAD(&wl->wlvif_list);
	INIT_LIST_HEAD(&wl->peers_list);

	wl->hw = hw;

	for (i = 0; i < NUM_TX_QUEUES; i++)
		for (j = 0; j < WL12XX_MAX_LINKS; j++)
			skb_queue_head_init(&wl->links[j].tx_queue[i]);

	skb_queue_head_init(&wl->deferred_rx_queue);
	skb_queue_head_init(&wl->deferred_tx_queue);

	INIT_DELAYED_WORK(&wl->elp_work, wl1271_elp_work);
	INIT_WORK(&wl->netstack_work, wl1271_netstack_work);
	INIT_WORK(&wl->tx_work, wl1271_tx_work);
	INIT_WORK(&wl->recovery_work, wl1271_recovery_work);
	INIT_DELAYED_WORK(&wl->scan_complete_work, wl1271_scan_complete_work);

	wl->freezable_wq = create_freezable_workqueue("wl12xx_wq");
	if (!wl->freezable_wq) {
		ret = -ENOMEM;
		goto err_hw;
	}

	wl->channel = WL1271_DEFAULT_CHANNEL;
	wl->rx_counter = 0;
	wl->power_level = WL1271_DEFAULT_POWER_LEVEL;
	wl->band = IEEE80211_BAND_2GHZ;
	wl->flags = 0;
	wl->sg_enabled = true;
	wl->hw_pg_ver = -1;
	wl->ap_ps_map = 0;
	wl->ap_fw_ps_map = 0;
	wl->quirks = 0;
	wl->platform_quirks = 0;
	wl->sched_scanning = false;
	wl->system_hlid = WL12XX_SYSTEM_HLID;
	wl->active_sta_count = 0;
	wl->fwlog_size = 0;
	init_waitqueue_head(&wl->fwlog_waitq);

	/* The system link is always allocated */
	__set_bit(WL12XX_SYSTEM_HLID, wl->links_map);

	memset(wl->tx_frames_map, 0, sizeof(wl->tx_frames_map));
	for (i = 0; i < ACX_TX_DESCRIPTORS; i++)
		wl->tx_frames[i] = NULL;

	spin_lock_init(&wl->wl_lock);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&wl->wake_lock, WAKE_LOCK_SUSPEND, "wl1271_wake");
	wake_lock_init(&wl->rx_wake, WAKE_LOCK_SUSPEND, "rx_wake");
	wake_lock_init(&wl->recovery_wake, WAKE_LOCK_SUSPEND, "recovery_wake");
#endif

	wl->state = WL1271_STATE_OFF;
	wl->fw_type = WL12XX_FW_TYPE_NONE;
	wl->saved_fw_type = WL12XX_FW_TYPE_NONE;
	mutex_init(&wl->mutex);

	/* Apply default driver configuration. */
	wl1271_conf_init(wl);

	order = get_order(WL1271_AGGR_BUFFER_SIZE);
	wl->aggr_buf = (u8 *)__get_free_pages(GFP_KERNEL, order);
	if (!wl->aggr_buf) {
		ret = -ENOMEM;
		goto err_wq;
	}

	wl->dummy_packet = wl12xx_alloc_dummy_packet(wl);
	if (!wl->dummy_packet) {
		ret = -ENOMEM;
		goto err_aggr;
	}

	/* Allocate one page for the FW log */
	wl->fwlog = (u8 *)get_zeroed_page(GFP_KERNEL);
	if (!wl->fwlog) {
		ret = -ENOMEM;
		goto err_dummy_packet;
	}

	return hw;

err_dummy_packet:
	dev_kfree_skb(wl->dummy_packet);

err_aggr:
	free_pages((unsigned long)wl->aggr_buf, order);

err_wq:
	destroy_workqueue(wl->freezable_wq);

err_hw:
	wl1271_debugfs_exit(wl);
	ieee80211_free_hw(hw);

err_hw_alloc:

	return ERR_PTR(ret);
}

static int wl1271_free_hw(struct wl1271 *wl)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&wl->wake_lock);
	wake_lock_destroy(&wl->rx_wake);
	wake_lock_destroy(&wl->recovery_wake);
#endif
	/* Unblock any fwlog readers */
	mutex_lock(&wl->mutex);
	wl->fwlog_size = -1;
	wake_up_interruptible_all(&wl->fwlog_waitq);
	mutex_unlock(&wl->mutex);

	device_remove_bin_file(wl->dev, &fwlog_attr);

	device_remove_file(wl->dev, &dev_attr_hw_pg_ver);

	device_remove_file(wl->dev, &dev_attr_bt_coex_state);
	free_page((unsigned long)wl->fwlog);
	dev_kfree_skb(wl->dummy_packet);
	free_pages((unsigned long)wl->aggr_buf,
			get_order(WL1271_AGGR_BUFFER_SIZE));

	wl1271_debugfs_exit(wl);

	vfree(wl->fw);
	wl->fw = NULL;
	wl->saved_fw_type = WL12XX_FW_TYPE_NONE;
	kfree(wl->nvs);
	wl->nvs = NULL;

	kfree(wl->fw_status);
	kfree(wl->tx_res_if);
	destroy_workqueue(wl->freezable_wq);

	ieee80211_free_hw(wl->hw);

	return 0;
}

static irqreturn_t wl12xx_hardirq(int irq, void *cookie)
{
	struct wl1271 *wl = cookie;
	unsigned long flags;

	wl1271_debug(DEBUG_IRQ, "IRQ");

	/* complete the ELP completion */
	spin_lock_irqsave(&wl->wl_lock, flags);
	set_bit(WL1271_FLAG_IRQ_RUNNING, &wl->flags);
	if (wl->elp_compl) {
		complete(wl->elp_compl);
		wl->elp_compl = NULL;
	}

	if (test_bit(WL1271_FLAG_SUSPENDED, &wl->flags)) {
		/* don't enqueue a work right now. mark it as pending */
		set_bit(WL1271_FLAG_PENDING_WORK, &wl->flags);
		wl1271_debug(DEBUG_IRQ, "should not enqueue work");
		disable_irq_nosync(wl->irq);
		pm_wakeup_event(wl->dev, 0);
#ifdef CONFIG_HAS_WAKELOCK
		if (!test_and_set_bit(WL1271_FLAG_WAKE_LOCK, &wl->flags))
			wake_lock(&wl->wake_lock);
#endif
		spin_unlock_irqrestore(&wl->wl_lock, flags);
		return IRQ_HANDLED;
	}
#ifdef CONFIG_HAS_WAKELOCK
	if (!test_and_set_bit(WL1271_FLAG_WAKE_LOCK, &wl->flags))
		wake_lock(&wl->wake_lock);
#endif
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	return IRQ_WAKE_THREAD;
}

static int __devinit wl12xx_probe(struct platform_device *pdev)
{
	struct wl12xx_platform_data *pdata = pdev->dev.platform_data;
	struct ieee80211_hw *hw;
	struct wl1271 *wl;
	unsigned long irqflags;
	int ret = -ENODEV;

	hw = wl1271_alloc_hw();
	if (IS_ERR(hw)) {
		wl1271_error("can't allocate hw");
		ret = PTR_ERR(hw);
		goto out;
	}

	wl = hw->priv;
	wl->irq = platform_get_irq(pdev, 0);
	if (wl->ref_clock < 0)
		wl->ref_clock = pdata->board_ref_clock;
	if (wl->tcxo_clock < 0)
		wl->tcxo_clock = pdata->board_tcxo_clock;
	wl->platform_quirks = pdata->platform_quirks;
	wl->set_power = pdata->set_power;
	wl->dev = &pdev->dev;
	wl->if_ops = pdata->ops;

	platform_set_drvdata(pdev, wl);

	if (wl->platform_quirks & WL12XX_PLATFORM_QUIRK_EDGE_IRQ)
		irqflags = IRQF_TRIGGER_RISING;
	else
		irqflags = IRQF_TRIGGER_HIGH | IRQF_ONESHOT;

	ret = request_threaded_irq(wl->irq, wl12xx_hardirq, wl12xx_irq,
				   irqflags,
				   pdev->name, wl);
	if (ret < 0) {
		wl1271_error("request_irq() failed: %d", ret);
		goto out_free_hw;
	}

	ret = enable_irq_wake(wl->irq);
	if (!ret) {
		wl->irq_wake_enabled = true;
		device_init_wakeup(wl->dev, 1);
		if (pdata->pwr_in_suspend) {
			hw->wiphy->wowlan.flags = WIPHY_WOWLAN_ANY;
			hw->wiphy->wowlan.n_patterns = WL1271_MAX_RX_FILTERS;
			hw->wiphy->wowlan.pattern_min_len = 1;
			hw->wiphy->wowlan.pattern_max_len =
				WL1271_RX_FILTER_MAX_PATTERN_SIZE;
		}

	}
	disable_irq(wl->irq);

	ret = wl1271_init_ieee80211(wl);
	if (ret)
		goto out_irq;

	ret = wl1271_register_hw(wl);
	if (ret)
		goto out_irq;

	/* Create sysfs file to control bt coex state */
	ret = device_create_file(wl->dev, &dev_attr_bt_coex_state);
	if (ret < 0) {
		wl1271_error("failed to create sysfs file bt_coex_state");
		goto out_irq;
	}

	/* Create sysfs file to get HW PG version */
	ret = device_create_file(wl->dev, &dev_attr_hw_pg_ver);
	if (ret < 0) {
		wl1271_error("failed to create sysfs file hw_pg_ver");
		goto out_bt_coex_state;
	}

	/* Create sysfs file for the FW log */
	ret = device_create_bin_file(wl->dev, &fwlog_attr);
	if (ret < 0) {
		wl1271_error("failed to create sysfs file fwlog");
		goto out_hw_pg_ver;
	}

	return 0;

out_hw_pg_ver:
	device_remove_file(wl->dev, &dev_attr_hw_pg_ver);

out_bt_coex_state:
	device_remove_file(wl->dev, &dev_attr_bt_coex_state);

out_irq:
	free_irq(wl->irq, wl);

out_free_hw:
	wl1271_free_hw(wl);

out:
	return ret;
}

static int __devexit wl12xx_remove(struct platform_device *pdev)
{
	struct wl1271 *wl = platform_get_drvdata(pdev);

	if (wl->irq_wake_enabled) {
		device_init_wakeup(wl->dev, 0);
		disable_irq_wake(wl->irq);
	}
	wl1271_unregister_hw(wl);
	free_irq(wl->irq, wl);
	wl1271_free_hw(wl);

	return 0;
}

static const struct platform_device_id wl12xx_id_table[] __devinitconst = {
	{ "wl12xx", 0 },
	{  } /* Terminating Entry */
};
MODULE_DEVICE_TABLE(platform, wl12xx_id_table);

static struct platform_driver wl12xx_driver = {
	.probe		= wl12xx_probe,
	.remove		= __devexit_p(wl12xx_remove),
	.id_table	= wl12xx_id_table,
	.driver = {
		.name	= "wl12xx_driver",
		.owner	= THIS_MODULE,
	}
};

static int __init wl12xx_init(void)
{
	wl1271_info("driver version: %s", wl12xx_git_head);
	wl1271_info("compilation time: %s", wl12xx_timestamp);

	return platform_driver_register(&wl12xx_driver);
}
module_init(wl12xx_init);

static void __exit wl12xx_exit(void)
{
	platform_driver_unregister(&wl12xx_driver);
}
module_exit(wl12xx_exit);

u32 wl12xx_debug_level = DEBUG_NONE;
EXPORT_SYMBOL_GPL(wl12xx_debug_level);
module_param_named(debug_level, wl12xx_debug_level, uint, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(debug_level, "wl12xx debugging level");

module_param_named(fwlog, fwlog_param, charp, 0);
MODULE_PARM_DESC(keymap,
		 "FW logger options: continuous, ondemand, dbgpins or disable");

module_param(bug_on_recovery, bool, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(bug_on_recovery, "BUG() on fw recovery");

module_param_named(fref, fref_param, charp, 0);
MODULE_PARM_DESC(fref, "FREF clock: 19.2, 26, 26x, 38.4, 38.4x, 52");

module_param_named(tcxo, tcxo_param, charp, 0);
MODULE_PARM_DESC(tcxo,
		 "TCXO clock: 19.2, 26, 38.4, 52, 16.368, 32.736, 16.8, 33.6");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luciano Coelho <coelho@ti.com>");
MODULE_AUTHOR("Juuso Oikarinen <juuso.oikarinen@nokia.com>");
