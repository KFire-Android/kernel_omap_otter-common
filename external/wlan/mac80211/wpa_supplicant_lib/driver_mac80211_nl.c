/*
 * driver_mac80211_nl.c
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "common.h"
#include "driver.h"
#include "eloop.h"
#include "driver_wext.h"
#include "ieee802_11_defs.h"
#include "wpa_common.h"
#include "wpa_ctrl.h"
#include "wpa_supplicant_i.h"
#include "config_ssid.h"
#include "wpa_debug.h"
#include "linux_ioctl.h"
#include "driver_nl80211.h"

#define WPA_EVENT_DRIVER_STATE          "CTRL-EVENT-DRIVER-STATE "
#define DRV_NUMBER_SEQUENTIAL_ERRORS     4

#define WPA_PS_ENABLED   0
#define WPA_PS_DISABLED  1

#define BLUETOOTH_COEXISTENCE_MODE_ENABLED   0
#define BLUETOOTH_COEXISTENCE_MODE_DISABLED  1
#define BLUETOOTH_COEXISTENCE_MODE_SENSE     2


static int g_drv_errors = 0;
static int g_power_mode = 0;

int send_and_recv_msgs(struct wpa_driver_nl80211_data *drv, struct nl_msg *msg,
                       int (*valid_handler)(struct nl_msg *, void *),
                       void *valid_data);

static void wpa_driver_send_hang_msg(struct wpa_driver_nl80211_data *drv)
{
	g_drv_errors++;
	if (g_drv_errors > DRV_NUMBER_SEQUENTIAL_ERRORS) {
		g_drv_errors = 0;
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
	}
}

static int wpa_driver_toggle_btcoex_state(char state)
{
	int ret;
	int fd = open("/sys/devices/platform/wl1271/bt_coex_state", O_RDWR, 0);
	if (fd == -1)
		return -1;

	ret = write(fd, &state, sizeof(state));
	close(fd);

	wpa_printf(MSG_DEBUG, "%s:  set btcoex state to '%c' result = %d", __func__,
		   state, ret);
	return ret;
}

static int wpa_driver_set_power_save(void *priv, int state)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -1;
	enum nl80211_ps_state ps_state;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
		    NL80211_CMD_SET_POWER_SAVE, 0);

	if (state == WPA_PS_ENABLED)
		ps_state = NL80211_PS_ENABLED;
	else
		ps_state = NL80211_PS_DISABLED;

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_PS_STATE, ps_state);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret < 0)
		wpa_printf(MSG_ERROR, "nl80211: Set power mode fail: %d", ret);
nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

#define MAX_PATTERN_SIZE        256
#define MAX_MASK_SIZE           (MAX_PATTERN_SIZE/8)

/* Describes a single RX filter configuration */
struct rx_filter {
	/* name - A human recongmizable name for the filter */
	char *name;

	/* get_pattern_handler - A handler which enables the user to configure
	 * the pattern dynamically (For example filter according to the HW addr).
	 * If NULL the static pattern configured will be used.
	 * buf - the pattern will be copied to buf
	 * buflen - the size of buf
	 * arg - A generic input argumet which can be passed to the handler
	 */
	int (* get_pattern_handler) (u8 *buf, int buflen, void* arg);

	/* pattern - A static pattern to filter
	 * This array contains the bytes of the pattern. The mask field
	 * indicates which bytes should be used in the filter and which
	 * can be discarded
	 */
	u8 pattern[MAX_PATTERN_SIZE];

	/* pattern_len - The number of bytes used in pattern */
	u8 pattern_len;

	/* mask - A bit mask indicating which bytes in pattern should be
	 * used for filtering. Each bit here corresponds to a byte in pattern
	 */
	u8 mask[MAX_MASK_SIZE];

	/* mask_len - The number of bytes used in mask */
	u8 mask_len;
};

static u8 *nl80211_rx_filter_get_pattern(struct rx_filter *filter, void *arg)
{
	if (filter->get_pattern_handler) {
		if (filter->get_pattern_handler(filter->pattern,
					        filter->pattern_len, arg)) {
			return NULL;
		}
	}

	return filter->pattern;
}

static int nl80211_self_filter_get_pattern_handler(u8 *buf, int buflen, void *arg)
{
	int ret;
	struct i802_bss *bss = (struct i802_bss *)arg;

	ret = linux_get_ifhwaddr(bss->drv->ioctl_sock,
				 bss->ifname, buf);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to get own HW addr (%d)", ret);
		return -1;
	}
	return 0;
}

static struct rx_filter rx_filters[] = {
	/* ID 0 */
	{.name = "self",
	 .pattern = {},
	 .pattern_len = 6,
	 .mask = { BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) },
	 .mask_len = 1,
	 .get_pattern_handler = nl80211_self_filter_get_pattern_handler,
	},

	/* ID 1 */
	{.name = "bcast",
	 .pattern = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
	 .pattern_len = 6,
	 .mask = { BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) },
	 .mask_len = 1,
	},

	/* ID 2 */
	{.name = "ipv4mc",
	 .pattern = {0x01,0x00,0x5E},
	 .pattern_len = 3,
	 .mask = { BIT(0) | BIT(1) | BIT(2) },
	 .mask_len = 1,
	},

	/* ID 3 */
	{.name = "ipv6mc",
	 .pattern = {0x33,0x33},
	 .pattern_len = 2,
	 .mask = { BIT(0) | BIT(1) },
	 .mask_len = 1,
	},

	/* ID 4 */
	{.name = "dhcp",
	 .pattern = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0   , 0   ,
		     0   , 0   , 0   , 0   , 0   , 0   , 0x45, 0   ,
		     0   , 0   , 0   , 0   , 0   , 0   , 0   , 0x11,
		     0   , 0   , 0   , 0   , 0   , 0   , 0   , 0   ,
		     0   , 0   , 0   , 0   , 0x00, 0x44},
	 .pattern_len = 38,
	 .mask = { BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5),
		   BIT(6),                            	/* OCTET 2 */
		   BIT(7),                            	/* OCTET 3 */
		   0,                                 	/* OCTET 4 */
		   BIT(4) | BIT(5) },                 	/* OCTET 5 */
	 .mask_len = 5,
	},
	/* ID 5 */
	{.name = "arp",
	 .pattern = {0   , 0   , 0   , 0   , 0   , 0   , 0   , 0   ,
		     0   , 0   , 0   , 0   , 0x08, 0x06},
	 .pattern_len = 14,
	 .mask = { 0,                                    /* OCTET 1 */
		   BIT(4) | BIT(5) },                    /* OCTET 2 */
	 .mask_len = 2,
	},

	/* ID 6 */
	{.name = "ssdp",
	 .pattern = {0x01, 0x00, 0x5E, 0   , 0   , 0   , 0   , 0   ,
		     0   , 0   , 0   , 0   , 0   , 0   , 0x45, 0   ,
		     0   , 0   , 0   , 0   , 0   , 0   , 0   , 0x11,
		     0   , 0   , 0   , 0   , 0   , 0   , 0xEF, 0xFF,
		     0xFF, 0xFA, 0   , 0   , 0x07, 0x6C},
	 .pattern_len = 38,
	 .mask = { BIT(0) | BIT(1) | BIT(2),             /* OCTET 1 */
		   BIT(6),                               /* OCTET 2 */
		   BIT(7),                               /* OCTET 3 */
		   BIT(6) | BIT(7),                      /* OCTET 4 */
		   BIT(0) | BIT(1) | BIT(4) | BIT(5) },  /* OCTET 5 */
	 .mask_len = 5,
	},
};

#define NR_RX_FILTERS			(sizeof(rx_filters) / sizeof(struct rx_filter))

static int nl80211_set_wowlan_triggers(struct i802_bss *bss, int enable)
{
	struct nl_msg *msg, *pats = NULL;
	struct nlattr *wowtrig, *pat;
	int i, ret = -1;
	int filters;

	bss->drv->wowlan_enabled = !!enable;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	genlmsg_put(msg, 0, 0, genl_family_get_id(bss->drv->nl80211), 0,
		    0, NL80211_CMD_SET_WOWLAN, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->drv->first_bss.ifindex);
	wowtrig = nla_nest_start(msg, NL80211_ATTR_WOWLAN_TRIGGERS);

	if (!wowtrig) {
		ret = -ENOBUFS;
		goto nla_put_failure;
	}

	if (!enable) {
		NLA_PUT_FLAG(msg, NL80211_WOWLAN_TRIG_ANY);
	} else {
		pats = nlmsg_alloc();
		if (!pats) {
			ret = -ENOMEM;
			goto nla_put_failure;
		}

		/* In ginger filter 0 and 1 are always set but in ICS we
		 * only enable unicast. Make sure to always set it, otherwise
		 * unicast packets will be dropped.
		 * bcast packets are dropped and handled by the firmware */

		filters = bss->drv->wowlan_triggers |= 1;

		for (i = 0; i < NR_RX_FILTERS; i++) {
			if (filters & (1 << i)) {
				struct rx_filter *rx_filter = &rx_filters[i];
				int patnr = 1;
				u8 *pattern = nl80211_rx_filter_get_pattern(rx_filter,bss);

				if (!pattern)
					continue;

				pat = nla_nest_start(pats, patnr++);
				NLA_PUT(pats, NL80211_WOWLAN_PKTPAT_MASK,
					rx_filter->mask_len,
					rx_filter->mask);
				NLA_PUT(pats, NL80211_WOWLAN_PKTPAT_PATTERN,
					rx_filter->pattern_len,
					pattern);

				nla_nest_end(pats, pat);
			}
		}
	}

	if (pats)
		nla_put_nested(msg, NL80211_WOWLAN_TRIG_PKT_PATTERN, pats);

	nla_nest_end(msg, wowtrig);

	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL);

	if (ret < 0)
		wpa_printf(MSG_ERROR, "Failed to set WoWLAN trigger:%d\n", ret);

	if (pats)
		nlmsg_free(pats);

	return 0;

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

static int nl80211_toggle_wowlan_trigger(struct i802_bss *bss, int nr,
					 int enabled)
{
	if (nr >= NR_RX_FILTERS) {
		wpa_printf(MSG_ERROR, "Unknown filter: %d\n", nr);
		return -1;
	}

	if (enabled)
		bss->drv->wowlan_triggers |= 1 << nr;
	else
		bss->drv->wowlan_triggers &= ~(1 << nr);

	if (bss->drv->wowlan_enabled)
		nl80211_set_wowlan_triggers(bss, 1);

	return 0;
}

static int nl80211_parse_wowlan_trigger_nr(char *s)
{
	long i;
	char *endp;

	i = strtol(s, &endp, 10);

	if(endp == s)
		return -1;
	return i;
}

static int nl80211_toggle_dropbcast(int enable)
{
	char filename[90];
	int rv;
	FILE *f;

	snprintf(filename, sizeof(filename) - 1,
		 "/sys/bus/platform/devices/wl12xx/drop_bcast");
	f = fopen(filename, "w");
	if (f < 0) {
		wpa_printf(MSG_DEBUG, "Could not open file %s: %s",
			   filename, strerror(errno));
		return -1;
	}

	rv = fprintf(f, "%d", enable);
	fclose(f);
	if (rv < 1) {
		wpa_printf(MSG_DEBUG, "Could not write to file %s: %s",
			   filename, strerror(errno));
		return -1;
	}

	return 0;
}

static int nl80211_dropbcast_get(char *buf, size_t buf_len)
{
	char filename[90], value[10], *pos;
	int f, rv;

	snprintf(filename, sizeof(filename) - 1,
		 "/sys/bus/platform/devices/wl12xx/drop_bcast");
	f = open(filename, O_RDONLY);
	if (f < 0) {
		wpa_printf(MSG_DEBUG, "Could not open file %s: %s",
			   filename, strerror(errno));
		return -1;
	}

	rv = read(f, value, sizeof(value) - 1);
	close(f);
	if (rv < 0) {
		wpa_printf(MSG_DEBUG, "Could not read file %s: %s",
			   filename, strerror(errno));
		return -1;
	}

	value[rv] = '\0';
	pos = os_strchr(value, '\n');
	if (pos)
		*pos = '\0';

	return snprintf(buf, buf_len, "Drop bcast = %s\n", value);
}

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
				  size_t buf_len )
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ifreq ifr;
	int ret = 0;

	if (os_strcasecmp(cmd, "STOP") == 0) {
		linux_set_iface_flags(drv->ioctl_sock, bss->ifname, 0);
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STOPPED");
	} else if (os_strcasecmp(cmd, "START") == 0) {
		linux_set_iface_flags(drv->ioctl_sock, bss->ifname, 1);
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STARTED");
	} else if (os_strcasecmp(cmd, "RELOAD") == 0) {
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
	} else if (os_strncasecmp(cmd, "POWERMODE ", 10) == 0) {
		int mode;
		mode = atoi(cmd + 10);
		ret = wpa_driver_set_power_save(priv, mode);
		if (ret < 0) {
			wpa_driver_send_hang_msg(drv);
		} else {
			g_power_mode = mode;
			g_drv_errors = 0;
		}
	} else if (os_strncasecmp(cmd, "GETPOWER", 8) == 0) {
		ret = os_snprintf(buf, buf_len, "POWERMODE = %d\n", g_power_mode);
	} else if (os_strncasecmp(cmd, "BTCOEXMODE ", 11) == 0) {
		int mode = atoi(cmd + 11);
		if (mode == BLUETOOTH_COEXISTENCE_MODE_DISABLED) { /* disable BT-coex */
			ret = wpa_driver_toggle_btcoex_state('0');
		} else if (mode == BLUETOOTH_COEXISTENCE_MODE_SENSE) { /* enable BT-coex */
			ret = wpa_driver_toggle_btcoex_state('1');
		} else {
			wpa_printf(MSG_DEBUG, "invalid btcoex mode: %d", mode);
			ret = -1;
		}
	} else if (os_strcasecmp(cmd, "MACADDR") == 0) {
		u8 macaddr[ETH_ALEN] = {};

		ret = linux_get_ifhwaddr(drv->ioctl_sock, bss->ifname, macaddr);
		if (!ret)
			ret = os_snprintf(buf, buf_len,
					  "Macaddr = " MACSTR "\n", MAC2STR(macaddr));
	} else if( os_strncasecmp(cmd, "RXFILTER-ADD ", 13) == 0 ) {
		int i = nl80211_parse_wowlan_trigger_nr(cmd + 13);
		if(i < 0)
			return i;
		return nl80211_toggle_wowlan_trigger(bss, i, 1);
	} else if( os_strncasecmp(cmd, "RXFILTER-REMOVE ", 16) == 0 ) {
		int i = nl80211_parse_wowlan_trigger_nr(cmd + 16);
		if(i < 0)
			return i;
		return nl80211_toggle_wowlan_trigger(bss, i, 0);
	} else if( os_strcasecmp(cmd, "RXFILTER-START") == 0 ) {
		return nl80211_set_wowlan_triggers(bss, 1);
	} else if( os_strcasecmp(cmd, "RXFILTER-STOP") == 0 ) {
		return nl80211_set_wowlan_triggers(bss, 0);
	} else if( os_strncasecmp(cmd, "DROPBCAST", 9) == 0 ) {
		char *value = cmd + 10;

		if (!os_strcasecmp(value, "ENABLE") ||
		    !os_strcasecmp(value, "1")) {
			ret = nl80211_toggle_dropbcast(1);
		} else if (!os_strcasecmp(value, "DISABLE") ||
			   !os_strcasecmp(value, "0")) {
			ret = nl80211_toggle_dropbcast(0);
		} else if (!os_strcasecmp(value, "GET") ||
			   !os_strlen(value)) {
			ret = nl80211_dropbcast_get(buf, buf_len);
		} else {
			wpa_printf(MSG_ERROR,
				   "Invalid parameter for DROPBCAST: %s",
				   value);
			ret = -1;
		}
	} else if( os_strncasecmp(cmd, "SETBAND ", 8) == 0 ) {
		int val = atoi(cmd + 8);
		ret = 0;
		if ( val < 0 || val > 2 )
			ret = -1;
	} else {
		wpa_printf(MSG_INFO, "%s: Unsupported command %s", __func__, cmd);
	}
	return ret;
}
