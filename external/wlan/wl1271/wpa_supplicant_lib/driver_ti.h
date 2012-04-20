/* 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _DRIVER_TI_H_
#define _DRIVER_TI_H_

#include "wireless_copy.h"
#include "common.h"
#include "driver.h"
#include "l2_packet.h"
#include "eloop.h"
#include "priv_netlink.h"
#include "driver_wext.h"
#include "wpa_ctrl.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#ifdef WPA_SUPPLICANT_VER_0_6_X
#include "ieee802_11_defs.h"
#else
#include "wpa.h"
#endif
#include "cu_ostypes.h"
#include "STADExternalIf.h"
#include "convert.h"
#include "shlist.h"

#define TIWLAN_DRV_NAME         "tiwlan0"

#define NUMBER_SCAN_CHANNELS_FCC        11
#define NUMBER_SCAN_CHANNELS_ETSI       13
#define NUMBER_SCAN_CHANNELS_MKK1       14

#define RX_SELF_FILTER			0
#define RX_BROADCAST_FILTER		1
#define RX_IPV4_MULTICAST_FILTER	2
#define RX_IPV6_MULTICAST_FILTER	3

#define MAX_NUMBER_SEQUENTIAL_ERRORS	4

typedef enum {
	BLUETOOTH_COEXISTENCE_MODE_ENABLED = 0,
	BLUETOOTH_COEXISTENCE_MODE_DISABLED,
	BLUETOOTH_COEXISTENCE_MODE_SENSE
} EUIBTCoexMode;

struct wpa_driver_ti_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int ioctl_sock;
	u8 own_addr[ETH_ALEN];          /* MAC address of WLAN interface */
	int driver_is_loaded;           /* TRUE/FALSE flag if driver is already loaded and can be accessed */
	int scan_type;                  /* SCAN_TYPE_NORMAL_ACTIVE or  SCAN_TYPE_NORMAL_PASSIVE */
	int force_merge_flag;		/* Force scan results merge */
	int scan_channels;              /* Number of allowed scan channels */
	unsigned int link_speed;        /* Link Speed */
	u32 btcoex_mode;		/* BtCoex Mode */
	int last_scan;			/* Last scan type */
	SHLIST scan_merge_list;		/* Previous scan list */
#ifdef CONFIG_WPS
	struct wpabuf *probe_req_ie;    /* Store the latest probe_req_ie for WSC */
#endif
	int errors;			/* Number of sequential errors */
};
#endif
