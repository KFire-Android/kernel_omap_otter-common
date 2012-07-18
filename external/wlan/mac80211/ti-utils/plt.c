/*
 * PLT utility for wireless chip supported by TI's driver wl12xx
 *
 * See README and COPYING for more details.
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <linux/wireless.h>
#include <linux/ethtool.h>
#include "nl80211.h"

#include "calibrator.h"
#include "plt.h"
#include "ini.h"
#include "nvs.h"

#define ZERO_MAC	"00:00:00:00:00:00"

#ifndef SIOCETHTOOL
#define SIOCETHTOOL     0x8946
#endif

SECTION(plt);

#define CMDBUF_SIZE 200
static int insmod(char *filename)
{
	int ret;
	char cmd[CMDBUF_SIZE];
	snprintf(cmd, CMDBUF_SIZE, "%s %s", INSMOD_PATH, filename);
	ret = system(cmd);
	if (ret)
		fprintf(stderr, "Failed to load kernel module using command %s\n", cmd);
	return ret;
}

static int rmmod(char *name)
{
	char cmd[CMDBUF_SIZE];
	char *tmp;
	int i, ret;

	/* "basename" */
	tmp = strrchr(name, '/');
	if (!tmp)
		tmp = name;
	else
		tmp++;

	tmp = strdup(tmp);
	if (!tmp)
		return -ENOMEM;

	/* strip trailing .ko if there */
	i = strlen(tmp);
	if (i < 4) {
		ret = -EINVAL;
		goto out;
	}
	if (!strcmp(tmp + i - 3, ".ko"))
		tmp[i-3] = 0;

	snprintf(cmd, CMDBUF_SIZE, "%s %s", RMMOD_PATH, tmp);
	ret = system(cmd);
	if (ret)
		fprintf(stderr, "Failed to remove kernel module using command %s\n", cmd);
out:
	free(tmp);
	return ret;
}

static int plt_power_mode(struct nl80211_state *state, struct nl_cb *cb,
			  struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	unsigned int pmode;

	if (argc != 1) {
		fprintf(stderr, "%s> Missing arguments\n", __func__);
		return 2;
	}

	if (strcmp(argv[0], "on") == 0)
		pmode = 1;
	else if (strcmp(argv[0], "off") == 0)
		pmode = 0;
	else {
		fprintf(stderr, "%s> Invalid parameter\n", __func__);
		return 2;
	}

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_SET_PLT_MODE);
	NLA_PUT_U32(msg, WL1271_TM_ATTR_PLT_MODE, pmode);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, power_mode, "<on|off>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_power_mode,
	"Set PLT power mode\n");

static int plt_tune_channel(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_channel_tune prms;

	if (argc < 1 || argc > 2)
		return 1;

	prms.test.id = TEST_CMD_CHANNEL_TUNE;
	prms.band = (unsigned char)atoi(argv[0]);
	prms.channel = (unsigned char)atoi(argv[1]);

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA,
		sizeof(struct wl1271_cmd_cal_channel_tune),
		&prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tune_channel, "<band> <channel>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tune_channel,
	"Set band and channel for PLT\n");

static int plt_ref_point(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_update_ref_point prms;

	if (argc < 1 || argc > 3)
		return 1;

	prms.test.id = TEST_CMD_UPDATE_PD_REFERENCE_POINT;
	prms.ref_detector = atoi(argv[0]);
	prms.ref_power = atoi(argv[1]);
	prms.sub_band = atoi(argv[2]);

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, ref_point, "<voltage> <power> <subband>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_ref_point,
	"Set reference point for PLT\n");

static int calib_valid_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[WL1271_TM_ATTR_MAX + 1];
	struct wl1271_cmd_cal_p2g *prms;
#if 0
	int i; unsigned char *pc;
#endif
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, WL1271_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	prms = (struct wl1271_cmd_cal_p2g *)nla_data(td[WL1271_TM_ATTR_DATA]);

	if (prms->radio_status) {
		fprintf(stderr, "Fail to calibrate ith radio status (%d)\n",
			(signed short)prms->radio_status);
		return 2;
	}
#if 0
	printf("%s> id %04x status %04x\ntest id %02x ver %08x len %04x=%d\n",
		__func__,
		prms->header.id, prms->header.status, prms->test.id,
		prms->ver, prms->len, prms->len);

		pc = (unsigned char *)prms->buf;
		printf("++++++++++++++++++++++++\n");
		for (i = 0; i < prms->len; i++) {
			if (i%0xf == 0)
				printf("\n");

			printf("%02x ", *(unsigned char *)pc);
			pc += 1;
		}
		printf("++++++++++++++++++++++++\n");
#endif
	printf("Writing calibration data to %s\n", (char*) arg);
	if (prepare_nvs_file(prms, arg)) {
		fprintf(stderr, "Fail to prepare calibrated NVS file\n");
		return 2;
	}
#if 0
	printf("\n\tThe NVS file (%s) is ready\n\tCopy it to %s and "
		"reboot the system\n\n",
		NEW_NVS_NAME, CURRENT_NVS_NAME);
#endif
	return NL_SKIP;
}

static void dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	printf("\n\tDriver %s\n\t"
		   "version %s\n\t"
		   "FW version %s\n\t"
		   "Bus info %s\n\t"
		   "HW version 0x%X\n",
		info->driver, info->version,
		info->fw_version, info->bus_info, regs->version);
}

int do_get_drv_info(char *dev_name, int *hw_ver)
{
	struct ifreq ifr;
	int fd, err;
	struct ethtool_drvinfo drvinfo;
	struct ethtool_regs *regs;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev_name);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		fprintf(stderr, "Cannot get control socket\n");
		return 1;
	}

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&drvinfo;
	err = ioctl(fd, SIOCETHTOOL, &ifr);
	if (err < 0) {
		fprintf(stderr, "Cannot get driver information\n");
		goto error_out;
	}

	regs = calloc(1, sizeof(*regs)+drvinfo.regdump_len);
	if (!regs) {
		fprintf(stderr, "Cannot allocate memory for register dump\n");
		goto error_out;
	}

	regs->cmd = ETHTOOL_GREGS;
	regs->len = drvinfo.regdump_len;
	ifr.ifr_data = (caddr_t)regs;
	err = ioctl(fd, SIOCETHTOOL, &ifr);
	if (err < 0) {
		fprintf(stderr, "Cannot get register dump\n");
		goto error_out2;
	}

	if (hw_ver)
		*hw_ver = regs->version;
	else
		dump_regs(&drvinfo, regs);
	free(regs);

	return 0;

error_out2:
	free(regs);

error_out:
	close(fd);

	return 1;
}

static int get_chip_arch(char *dev_name, enum wl12xx_arch *arch)
{
	int hw_ver, ret;

	ret = do_get_drv_info(dev_name, &hw_ver);
	if (ret)
		return 1;

	*arch = hw_ver >> 16;

	return 0;
}

static int do_nvs_ver21(struct nl_msg *msg, enum wl12xx_arch arch)
{
	struct nlattr *key;
	struct wl1271_cmd_set_nvs_ver prms;

	memset(&prms, 0, sizeof(struct wl1271_cmd_set_nvs_ver));

	if (arch == WL1271_ARCH)
		prms.test.id = TEST_CMD_SET_NVS_VERSION;
	else if(arch == WL128X_ARCH)
		prms.test.id = TEST_CMD_SET_NVS_VERSION - 1;
	else {
		fprintf(stderr, "Unkown arch %x\n", arch);
		return 1;
	}

	prms.nvs_ver = NVS_VERSION_2_1;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}


static int plt_nvs_ver(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	enum wl12xx_arch arch = UNKNOWN_ARCH;
	int ret;

	if (argc < 1) {
		fprintf(stderr, "Missing device name\n");
		return 2;
	}

	ret = get_chip_arch(argv[0], &arch);
	if (ret || (arch == UNKNOWN_ARCH)) {
		fprintf(stderr, "Unknown chip arch\n");
		return 2;
	}

	return do_nvs_ver21(msg, arch);
}

COMMAND(plt, nvs_ver, "<device name>",
	NL80211_CMD_TESTMODE, 0, CIB_PHY, plt_nvs_ver,
	"Set NVS version\n");


static int plt_nvs_ver2(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	enum wl12xx_arch arch = UNKNOWN_ARCH;
	int ret;

	if (argc < 1) {
		fprintf(stderr, "Missing device name\n");
		return 2;
	}

	ret = sscanf(argv[0], "%x", &arch);
	if(ret != 1) {
		fprintf(stderr, "Unknown chip arch\n");
		return 2;
	}

	return do_nvs_ver21(msg, arch);
}

COMMAND(plt, nvs_ver, "<arch>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_nvs_ver2,
	"Set NVS version\n");

static int plt_tx_bip(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_p2g prms;
	int i;
	char nvs_path[PATH_MAX];

	if (argc < 8) {
		fprintf(stderr, "%s> Missing arguments\n", __func__);
		return 2;
	}

	if (argc > 8)
		strncpy(nvs_path, argv[8], strlen(argv[8]));
	else
		nvs_path[0] = '\0';

	memset(&prms, 0, sizeof(struct wl1271_cmd_cal_p2g));

	prms.test.id = TEST_CMD_P2G_CAL;
	for (i = 0; i < 8; i++)
		prms.sub_band_mask |= (atoi(argv[i]) & 0x1)<<i;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);
	NLA_PUT_U8(msg, WL1271_TM_ATTR_ANSWER, 1);

	nla_nest_end(msg, key);

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, calib_valid_handler, nvs_path);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_bip,
	"<0|1> <0|1> <0|1> <0|1> <0|1> <0|1> <0|1> <0|1> [<nvs file>]",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_bip,
	"Do calibrate\n");

static int plt_tx_tone(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_tx_tone prms;

	if (argc < 2) {
		fprintf(stderr, "%s> Missing arguments\n", __func__);
		return 2;
	}

	memset(&prms, 0, sizeof(struct wl1271_cmd_cal_tx_tone));

	prms.test.id = TEST_CMD_TELEC;

	prms.tone_type = atoi(argv[0]);
	if (prms.tone_type < 1 || prms.tone_type > 2) {
		fprintf(stderr, "%s> Invalit tone type parameter %d\n",
			__func__, prms.tone_type);
		return 2;
	}

	prms.power = atoi(argv[1]);
	if (prms.power > 10000) {
		fprintf(stderr, "%s> Invalit power parameter %d\n",
			__func__, prms.power);
		return 2;
	}

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_tone, "<tone type 1|2> <power 0 - 10000>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_tone,
	"Do command tx_tone to transmit a tone\n");

static int plt_tx_cont(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms = {
		.src_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	};

	if (argc != 15)
		return 1;
#if 0
	printf("%s> delay (%d) rate (%08x) size (%d) amount (%d) power (%d) "
		"seed (%d) pkt_mode (%d) DCF (%d) GI (%d) preamble (%d) type "
		"(%d) scramble (%d) CLPC (%d), SeqNbrMode (%d) DestMAC (%s)\n",
		__func__,
		atoi(argv[0]),  atoi(argv[1]),  atoi(argv[2]),  atoi(argv[3]),
		atoi(argv[4]),  atoi(argv[5]),  atoi(argv[6]),  atoi(argv[7]),
		atoi(argv[8]),  atoi(argv[9]),  atoi(argv[10]), atoi(argv[11]),
		atoi(argv[12]), atoi(argv[13]), argv[14]
	);
#endif
	memset((void *)&prms, 0, sizeof(struct wl1271_cmd_pkt_params));

	prms.test.id = TEST_CMD_FCC;
	prms.delay = atoi(argv[0]);
	prms.rate = strtol(argv[1], NULL, 0);
	prms.size = (unsigned short)atoi(argv[2]);
	prms.amount = (unsigned short)atoi(argv[3]);
	prms.power = atoi(argv[4]);
	prms.seed = (unsigned short)atoi(argv[5]);
	prms.pkt_mode = (unsigned char)atoi(argv[6]);
	prms.dcf_enable = (unsigned char)atoi(argv[7]);
	prms.g_interval = (unsigned char)atoi(argv[8]);
	prms.preamble = (unsigned char)atoi(argv[9]);
	prms.type = (unsigned char)atoi(argv[10]);
	prms.scramble = (unsigned char)atoi(argv[11]);
	prms.clpc_enable = (unsigned char)atoi(argv[12]);
	prms.seq_nbr_mode = (unsigned char)atoi(argv[13]);
	str2mac(prms.dst_mac, argv[14]);

	if (get_mac_addr(0, prms.src_mac))
		fprintf(stderr, "fail to get MAC addr\n");

	printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
		prms.src_mac[0], prms.src_mac[1], prms.src_mac[2],
		prms.src_mac[3], prms.src_mac[4], prms.src_mac[5]);

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_cont, "<delay> <rate> <size> <amount> <power>\n\t\t<seed> "
	"<pkt mode> <DC on/off> <gi> <preamble>\n\t\t<type> <scramble> "
	"<clpc> <seq nbr mode> <dest mac>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_cont,
	"Start Tx Cont\n");

static int plt_tx_stop(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_STOP_TX;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_stop, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_stop,
	"Stop Tx Cont\n");

static int plt_start_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_START;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, start_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_start_rx_statcs,
	"Start Rx statistics collection\n");

static int plt_stop_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_STOP;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, stop_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_stop_rx_statcs,
	"Stop Rx statistics collection\n");

static int plt_reset_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_RESET;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, reset_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_reset_rx_statcs,
	"Reset Rx statistics collection\n");

static int display_rx_statcs(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[WL1271_TM_ATTR_MAX + 1];
	struct wl1271_radio_rx_statcs *prms;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, WL1271_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	prms =
		(struct wl1271_radio_rx_statcs *)
			nla_data(td[WL1271_TM_ATTR_DATA]);

	printf("\n\tTotal number of pkts\t- %d\n\tAccepted pkts\t\t- %d\n\t"
		"FCS error pkts\t\t- %d\n\tAddress mismatch pkts\t- %d\n\t"
		"Average SNR\t\t- % d dBm\n\tAverage RSSI\t\t- % d dBm\n\n",
		prms->base_pkt_id, prms->rx_path_statcs.nbr_rx_valid_pkts,
		prms->rx_path_statcs.nbr_rx_fcs_err_pkts,
		prms->rx_path_statcs.nbr_rx_plcp_err_pkts,
		(signed short)prms->rx_path_statcs.ave_snr/8,
		(signed short)prms->rx_path_statcs.ave_rssi/8);

	return NL_SKIP;
}

static int plt_get_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_radio_rx_statcs prms;

	prms.test.id = TEST_CMD_RX_STAT_GET;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);
	NLA_PUT_U8(msg, WL1271_TM_ATTR_ANSWER, 1);

	nla_nest_end(msg, key);

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, display_rx_statcs, NULL);

	/* Important: needed gap between tx_start and tx_get */
	sleep(2);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, get_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_get_rx_statcs,
	"Get Rx statistics\n");

static int plt_rx_statistics(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	int ret;

	/* power mode on */
	{
		char *prms[4] = { "wlan0", "plt", "power_mode", "on" };

		ret = handle_cmd(state, II_NETDEV, 4, prms);
		if (ret < 0) {
			fprintf(stderr, "Fail to set PLT power mode on\n");
			return 1;
		}
	}

	/* start_rx_statcs */
	{
		char *prms[3] = { "wlan0", "plt", "start_rx_statcs" };

		ret = handle_cmd(state, II_NETDEV, 3, prms);
		if (ret < 0) {
			fprintf(stderr, "Fail to start Rx statistics\n");
			goto fail_out;
		}
	}

	/* get_rx_statcs */
	{
		int err;
		char *prms[3] = { "wlan0", "plt", "get_rx_statcs" };

		err = handle_cmd(state, II_NETDEV, 3, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to get Rx statistics\n");
			ret = err;
		}
	}


	/* stop_rx_statcs */
	{
		int err;
		char *prms[3] = { "wlan0", "plt", "stop_rx_statcs" };

		err = handle_cmd(state, II_NETDEV, 3, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to stop Rx statistics\n");
			ret = err;
		}
	}

fail_out:
	/* power mode off */
	{
		int err;
		char *prms[4] = { "wlan0", "plt", "power_mode", "off"};

		err = handle_cmd(state, II_NETDEV, 4, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to set PLT power mode on\n");
			return 1;
		}
	}

	if (ret < 0)
		return 1;

	return 0;
}

COMMAND(plt, rx_statistics, NULL, 0, 0, CIB_NONE, plt_rx_statistics,
	"Get Rx statistics\n");

static int plt_do_power_on(struct nl80211_state *state, char *devname)
{
	int err;
	char *pm_on[4] = { devname, "plt", "power_mode", "on" };

	err = handle_cmd(state, II_NETDEV, ARRAY_SIZE(pm_on), pm_on);
	if (err < 0)
		fprintf(stderr, "Fail to set PLT power mode on\n");

	return err;
}

static int plt_do_power_off(struct nl80211_state *state, char *devname)
{
	int err;
	char *prms[4] = { devname, "plt", "power_mode", "off"};

	err = handle_cmd(state, II_NETDEV, ARRAY_SIZE(prms), prms);
	if (err < 0)
		fprintf(stderr, "Failed to set PLT power mode on\n");

	return err;
}


static int plt_do_calibrate(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int single_dual, char *nvs_file,
			char *devname, enum wl12xx_arch arch)
{
	int ret = 0, err;

	/* tune channel */
	{
		char *tune[5] = {
			devname, "plt", "tune_channel", "0", "7"
		};

		err = handle_cmd(state, II_NETDEV, ARRAY_SIZE(tune), tune);
		if (err < 0) {
			fprintf(stderr, "Fail to tune channel\n");
			ret = err;
			goto fail_out;
		}
	}

	/* Set nvs version 2.1 */
	if (arch == UNKNOWN_ARCH) {
		fprintf(stderr, "Unknown arch. Not setting nvs ver 2.1");
	}
	else {
		size_t ret;
		char archstr[5] = "";
		char *prms[4] = {
			"wlan0", "plt", "nvs_ver", archstr
		};

		ret = snprintf(archstr, sizeof(archstr), "%x", arch);
		if (ret > sizeof(archstr)) {
			fprintf(stderr, "Bad arch\n");
			goto fail_out;
		}

		printf("Using nvs version 2.1\n");
		err = handle_cmd(state, II_NETDEV, 4, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to set nvs ver 2.1\n");
			ret = err;
		}
	}

	/* calibrate it */
	{
		char *prms[12] = {
			devname, "plt", "tx_bip", "1", "0", "0", "0",
			"0", "0", "0", "0", nvs_file
		};
		printf("Calibrate %s\n", nvs_file);

		/* set flags in case of dual band */
		if (single_dual) {
			prms[4] = prms[5] = prms[6] = prms[7] = prms[8] =
				prms[9] = prms[10] = "1";
		}

		err = handle_cmd(state, II_NETDEV, ARRAY_SIZE(prms), prms);
		if (err < 0) {
			fprintf(stderr, "Failed to calibrate\n");
			ret = err;
		}
	}

fail_out:
	if (ret < 0)
		return 1;

	return 0;
}

static int plt_calibrate(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	int ret, err;
	int single_dual = 0;

	if (argc > 2 && (strncmp(argv[2], "dual", 4) ==  0))
		single_dual = 1;	/* going for dual band calibration */
	else
		single_dual = 0;	/* going for single band calibration */


	err = plt_do_power_on(state, "wlan0");
	if (err < 0)
		goto out;

	err = plt_do_calibrate(state, cb, msg, single_dual, NEW_NVS_NAME,
			       "wlan0", UNKNOWN_ARCH);

	ret = plt_do_power_off(state, "wlan0");
	if (ret < 0)
		err = ret;
out:
	return err;
}

COMMAND(plt, calibrate, "[<single|dual>]", 0, 0, CIB_NONE,
	plt_calibrate, "Do calibrate for single or dual band chip\n");


static int plt_autocalibrate(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct wl12xx_common cmn = {
		.auto_fem = 0,
		.arch = UNKNOWN_ARCH,
		.parse_ops = NULL,
	};

	char *devname, *modpath, *inifile1, *macaddr;
	char *set_mac_prms[5];
	int single_dual = 0, res, fems_parsed;

	argc -= 2;
	argv += 2;

	if (argc < 4 || argc > 5) {
		return 1;
	}

	devname = *argv++;
	argc--;

	modpath = *argv++;
	argc--;

	inifile1 = *argv++;
	argc--;

	cmn.nvs_name = get_opt_nvsoutfile(argc--, argv++);

	if (argc) {
		macaddr = *argv++;
		argc--;
	} else {
		macaddr = NULL;
	}

	if (file_exist(cmn.nvs_name) >= 0) {
		fprintf(stderr, "nvs file %s. File already exists. Won't overwrite.\n", cmn.nvs_name);
		return 0;
	}

	/* Create ref nvs */
	if (read_ini(inifile1, &cmn)) {
		fprintf(stderr, "Failed to read ini file %s\n", inifile1);
		goto out_removenvs;
	}

	fems_parsed = cmn.fem0_bands + cmn.fem1_bands;

	/* Get nr bands from parsed ini */
	single_dual = ini_get_dual_mode(&cmn);

	if (single_dual == 0) {
		if (fems_parsed < 1 || fems_parsed > 2) {
			fprintf(stderr, "Incorrect number of FEM sections %d for single mode\n",
			        fems_parsed);
			return 1;
		}
	}
	else if (single_dual == 1) {
		if (fems_parsed < 2 && fems_parsed > 4) {
			fprintf(stderr, "Incorrect number of FEM sections %d for dual mode\n",
			        fems_parsed);
			return 1;
		}
	}
	else {
		fprintf(stderr, "Invalid value for TXBiPFEMAutoDetect %d",
		        single_dual);
		return 1;
	}

	/* I suppose you can have one FEM with 2.4 only and one in dual band
	   but it's more likely a mistake */
	if ((single_dual + 1) * (cmn.auto_fem + 1) != fems_parsed) {
		printf("WARNING: %d FEMS for %d bands with autofem %s looks "
			"like a strange configuration\n",
			fems_parsed, single_dual + 1,
			cmn.auto_fem ? "on" : "off");
	}

	cfg_nvs_ops(&cmn);

	if (create_nvs_file(&cmn)) {
		fprintf(stderr, "Failed to create reference NVS file\n");
		return 1;
	}

	/* Load module */
	res = insmod(modpath);
	if (res) {
		goto out_removenvs;
	}

	res = plt_do_power_on(state, devname);
	if (res < 0)
		goto out_rmmod;

	res = plt_do_calibrate(state, cb, msg, single_dual,
	                      cmn.nvs_name, devname, cmn.arch);
	if (res) {
		goto out_power_off;
	}

	set_mac_prms[0] = devname;
	set_mac_prms[1] = "plt";
	set_mac_prms[2] = "set_mac";
	set_mac_prms[3] = cmn.nvs_name;
	set_mac_prms[4] = macaddr;

	res = handle_cmd(state, II_NETDEV,
			 ARRAY_SIZE(set_mac_prms) - (!macaddr),
			 set_mac_prms);
	if (res) {
		goto out_power_off;
	}

	/* we can ignore the return value, because we rmmod anyway */
	plt_do_power_off(state, devname);
	rmmod(modpath);

	printf("Calibration done. ");
	if (cmn.fem0_bands) {
		printf("FEM0 has %d bands. ", cmn.fem0_bands);
	}
	if (cmn.fem1_bands) {
		printf("FEM1 has %d bands. ", cmn.fem1_bands);
	}

	printf("AutoFEM is %s. ", cmn.auto_fem ? "on" : "off");

	printf("Resulting nvs is %s\n",
	       cmn.nvs_name);
	return 0;

out_power_off:
	/* we can ignore the return value, because we rmmod anyway */
	plt_do_power_off(state, devname);
out_rmmod:
	rmmod(modpath);

out_removenvs:
	fprintf(stderr, "Calibration not complete. Removing half-baked nvs\n");
	unlink(cmn.nvs_name);
	return 0;

}
COMMAND(plt, autocalibrate, "<dev> <module path> <ini file1> <nvs file> "
	"[<MAC address>|from_fuse|default]", 0, 0, CIB_NONE, plt_autocalibrate,
	"Do automatic calibration.\n"
	"The MAC address value can be:\n"
	"from_fuse\ttry to read from the fuse ROM, if not available the command fails\n"
	"default\t\twrite 00:00:00:00:00:00 to have the driver read from the fuse ROM,\n"
	"\t\t\tfails if not available\n"
	"00:00:00:00:00:00\tforce use of a zeroed MAC address (use with caution!)\n");

static int plt_get_mac_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[WL1271_TM_ATTR_MAX + 1];
	char *addr;
	int lower;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, WL1271_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		  nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	addr = (char *) nla_data(td[WL1271_TM_ATTR_DATA]);

	printf("BD_ADDR from fuse:\t0x%0x:0x%0x:0x%0x:0x%0x:0x%0x:0x%0x\n",
	       addr[0], addr[1], addr[2],
	       addr[3], addr[4], addr[5]);

	lower = (addr[3] << 16) + (addr[4] << 8) + addr[5];

	lower++;
	printf("First WLAN MAC:\t\t0x%0x:0x%0x:0x%0x:0x%0x:0x%0x:0x%0x\n",
	       addr[0], addr[1], addr[2],
	       (lower & 0xff0000) >> 16,
	       (lower & 0xff00) >> 8,
	       (lower & 0xff));

	lower++;
	printf("Second WLAN MAC:\t0x%0x:0x%0x:0x%0x:0x%0x:0x%0x:0x%0x\n",
	       addr[0], addr[1], addr[2],
	       (lower & 0xff0000) >> 16,
	       (lower & 0xff00) >> 8,
	       (lower & 0xff));

	return NL_SKIP;
}

static int plt_get_mac_from_fuse(struct nl_msg *msg, struct nl_cb *cb,
				 nl_recvmsg_msg_cb_t callback, void *arg)
{
	struct nlattr *key;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_GET_MAC);

	nla_nest_end(msg, key);

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback, arg);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

static int plt_get_mac(struct nl80211_state *state, struct nl_cb *cb,
		       struct nl_msg *msg, int argc, char **argv)
{
	if (argc != 0)
		return 1;

	return plt_get_mac_from_fuse(msg, cb, plt_get_mac_cb, NULL);
}
COMMAND(plt, get_mac, "",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_get_mac,
	"Read MAC address from the Fuse ROM.\n");

static int plt_set_mac_from_fuse_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[WL1271_TM_ATTR_MAX + 1];
	char mac[sizeof(ZERO_MAC)];
	char *addr;
	char *nvs_file = (char *) arg;
	int lower;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, WL1271_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		  nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	addr = (char *) nla_data(td[WL1271_TM_ATTR_DATA]);

	/*
	 * The first address is the BD_ADDR, the next is the first
	 * MAC.  Increment only the lower part, so we don't overflow
	 * to the OUI */
	lower = (addr[3] << 16) + (addr[4] << 8) + addr[5] + 1;

	snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
		 addr[0], addr[1], addr[2], (lower & 0xff0000) >> 16,
		 (lower & 0xff00) >> 8, (lower & 0xff));

	/* ignore the return value, since a message was already printed out */
	nvs_set_mac(nvs_file, mac);

	return NL_SKIP;
}

static int plt_set_mac_default_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	char *nvs_file = (char *) arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	/*
	 * No need to parse, we just need to know if the command
	 * worked (ie. the hardware supports MAC from fuse) so the
	 * driver can fetch it by itself.
	 */

	/* ignore the return value, since a message was already printed out */
	nvs_set_mac(nvs_file, ZERO_MAC);

	return NL_SKIP;
}

static int plt_set_mac_from_fuse(struct nl80211_state *state, struct nl_cb *cb,
				 struct nl_msg *msg, int argc, char **argv)
{
	return plt_get_mac_from_fuse(msg, cb, plt_set_mac_from_fuse_cb, argv[0]);
}
HIDDEN(plt, set_mac_from_fuse, "<nvs file>",
       NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_set_mac_from_fuse);

static int plt_set_mac_default(struct nl80211_state *state, struct nl_cb *cb,
			       struct nl_msg *msg, int argc, char **argv)
{
	return plt_get_mac_from_fuse(msg, cb, plt_set_mac_default_cb, argv[0]);
}
HIDDEN(plt, set_mac_default, "<nvs file>",
       NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_set_mac_default);

static int plt_set_mac(struct nl80211_state *state, struct nl_cb *cb,
		       struct nl_msg *msg, int argc, char **argv)
{
	char *nvs_file;

	if (argc < 4 || argc > 5)
		return 1;

	nvs_file = argv[3];

	if (argc == 4 || !strcmp(argv[4], "default")) {
		char *prms[] = { argv[0], argv[1], "set_mac_default",
				 nvs_file };

		return handle_cmd(state, II_NETDEV, ARRAY_SIZE(prms), prms);
	}

	if (!strcmp(argv[4], "from_fuse")) {
		char *prms[] = { argv[0], argv[1], "set_mac_from_fuse",
				 nvs_file };

		return handle_cmd(state, II_NETDEV, ARRAY_SIZE(prms), prms);
	}

	if (nvs_set_mac(nvs_file, argv[4]) != 0)
		return 1;

	return 0;
}
COMMAND(plt, set_mac, "<nvs file> [<MAC address>|from_fuse|default]",
	0, 0, CIB_NETDEV, plt_set_mac,
	"Set a MAC address to the NVS file.\n\n"
	"<MAC address>\tspecific address to use (XX:XX:XX:XX:XX:XX)\n"
	"from_fuse\ttry to read from the fuse ROM, if not available the command fails\n"
	"default\t\twrite 00:00:00:00:00:00 to have the driver read from the fuse ROM,\n"
	"\t\t\tfails if not available\n"
	"00:00:00:00:00:00\tforce use of a zeroed MAC address (use with caution!)\n");
