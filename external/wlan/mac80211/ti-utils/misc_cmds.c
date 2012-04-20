#include <stdbool.h>
#include <errno.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "calibrator.h"
#include "plt.h"
#include "ini.h"
#include "nvs.h"

SECTION(get);
SECTION(set);

static int get_nvs_mac(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	unsigned char mac_buff[12];
	char *fname;
	int fd;

	argc -= 2;
	argv += 2;

	fname = get_opt_nvsinfile(argc, argv);
	if (!fname)
		return 1;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("Error opening file for reading");
		return 1;
	}

	read(fd, mac_buff, 12);

	printf("MAC addr from NVS: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_buff[11], mac_buff[10], mac_buff[6],
		mac_buff[5], mac_buff[4], mac_buff[3]);

	close(fd);

	return 0;
}

COMMAND(get, nvs_mac, "[<nvs filename>]", 0, 0, CIB_NONE, get_nvs_mac,
	"Get MAC addr from NVS file (offline)");

/*
 * Sets MAC address in NVS.
 * The default value for MAC is random where 1 byte zero.
 */
static int set_nvs_mac(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	argc -= 2;
	argv += 2;

	if ((argc != 2) || (strlen(argv[1]) != 17))
		return 1;

	nvs_set_mac(argv[0], argv[1]);
	return 0;
}

COMMAND(set, nvs_mac, "<nvs file> <mac addr>", 0, 0, CIB_NONE, set_nvs_mac,
	"Set MAC addr in NVS file (offline), like XX:XX:XX:XX:XX:XX");

static int set_ref_nvs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct wl12xx_common cmn = {
		.arch = UNKNOWN_ARCH,
		.parse_ops = NULL
	};

	argc -= 2;
	argv += 2;

	if (argc < 1 || argc > 2)
		return 1;

	if (read_ini(*argv, &cmn)) {
		fprintf(stderr, "Fail to read ini file\n");
		return 1;
	}
	argv++;
	argc--;

	cfg_nvs_ops(&cmn);

	cmn.nvs_name = get_opt_nvsoutfile(argc, argv);
	if (create_nvs_file(&cmn)) {
		fprintf(stderr, "Fail to create reference NVS file\n");
		return 1;
	}

	printf("%04X", cmn.arch);

	return 0;
}

COMMAND(set, ref_nvs, "<ini file> [<nvs file>]", 0, 0, CIB_NONE, set_ref_nvs,
	"Create reference NVS file");

static int set_upd_nvs(struct nl80211_state *state, struct nl_cb *cb,
	struct nl_msg *msg, int argc, char **argv)
{
	char *infname = NULL, *outfname = NULL;
	struct wl12xx_common cmn = {
		.arch = UNKNOWN_ARCH,
		.parse_ops = NULL
	};

	argc -= 2;
	argv += 2;

	if (argc < 1)
		return 1;

	if (read_ini(*argv, &cmn)) {
		fprintf(stderr, "Fail to read ini file\n");
		return 1;
	}
	argc--;
	argv++;

	infname = get_opt_nvsinfile(argc, argv);
	if (!infname)
		return 1;

	if (argc) {
		argc--;
		argv++;
	}
	outfname = get_opt_nvsoutfile(argc, argv);
	if (!outfname)
		return 1;

	cfg_nvs_ops(&cmn);

	if (update_nvs_file(infname, outfname, &cmn)) {
		fprintf(stderr, "Fail to update NVS file\n");
		return 1;
	}
#if 0
	printf("\n\tThe updated NVS file (%s) is ready\n\tCopy it to %s and "
		"reboot the system\n\n", NEW_NVS_NAME, CURRENT_NVS_NAME);
#endif
	return 0;
}

COMMAND(set, upd_nvs, "<ini file> [<nvs infile>] [<nvs_outfile>]", 0, 0, CIB_NONE, set_upd_nvs,
	"Update values of a NVS from INI file");

static int get_dump_nvs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	char *fname = NULL;

	argc -= 2;
	argv += 2;

	fname = get_opt_nvsinfile(argc, argv);
	if (!fname)
		return 1;

	if (dump_nvs_file(fname)) {
		fprintf(stderr, "Fail to dump NVS file\n");
		return 1;
	}

	return 0;
}

COMMAND(get, dump_nvs, "[<nvs file>]", 0, 0, CIB_NONE, get_dump_nvs,
	"Dump NVS file, specified by option or current");

static int get_info_nvs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	char *fname;

	argc -= 2;
	argv += 2;

	fname = get_opt_nvsinfile(argc, argv);
	if(!fname)
		return 1;

	if (info_nvs_file(fname)) {
		fprintf(stderr, "Fail to read info from NVS file\n");
		return 1;
	}

	return 0;
}

COMMAND(get, info_nvs, "[<nvs file>]", 0, 0, CIB_NONE, get_info_nvs,
	"Print information from nvs file");

static int set_autofem(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	char *fname = NULL;
	int res;
	unsigned int val;
	struct wl12xx_common cmn = {
		.arch = UNKNOWN_ARCH,
		.parse_ops = NULL
	};

	argc -= 2;
	argv += 2;

	if (argc < 1) {
		fprintf(stderr, "Missing argument\n");
		return 2;
	}

	res = sscanf(argv[0], "%x", &val);
	if (res != 1 || val > 1) {
		fprintf(stderr, "Invalid argument\n");
		return 1;
	}
	argv++;
	argc--;

	fname = get_opt_nvsinfile(argc, argv);

	if (set_nvs_file_autofem(fname, val, &cmn)) {
		fprintf(stderr, "Fail to set AutoFEM\n");
		return 1;
	}

	return 0;
}

COMMAND(set, autofem, "<0-manual|1-auto> [<nvs file>]", 0, 0, CIB_NONE, set_autofem,
	"Set Auto FEM detection, where 0 - manual, 1 - auto detection");

static int set_fem_manuf(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	char *fname = NULL;
	int res;
	unsigned int val;
	struct wl12xx_common cmn = {
		.arch = UNKNOWN_ARCH,
		.parse_ops = NULL
	};

	argc -= 2;
	argv += 2;

	if (argc < 1) {
		fprintf(stderr, "Missing argument\n");
		return 2;
	}
	res = sscanf(argv[0], "%x", &val);
	if(res != 1 || val >= WL1271_INI_FEM_MODULE_COUNT) {
		fprintf(stderr, "Invalid argument\n");
		return 1;
	}
	argv++;
	argc--;

	fname = get_opt_nvsinfile(argc, argv);

	if (set_nvs_file_fem_manuf(fname, val, &cmn)) {
		fprintf(stderr, "Fail to set AutoFEM\n");
		return 1;
	}

	return 0;
}

COMMAND(set, fem_manuf, "<0|1> [<nvs file>]", 0, 0, CIB_NONE, set_fem_manuf,
	"Set FEM manufacturer");

static int get_drv_info(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	argc -= 2;
	argv += 2;

	if (argc < 1) {
		fprintf(stderr, "Missing argument (device name)\n");
		return 2;
	}

	return do_get_drv_info(argv[0], NULL);
}

COMMAND(get, drv_info, "<device name>", 0, 0, CIB_NONE, get_drv_info,
	"Get driver information: PG version");

static int get_hw_version(struct nl80211_state *state, struct nl_cb *cb,
			  struct nl_msg *msg, int argc, char **argv)
{
	int ret, chip_id = 0;

	argc -= 2;
	argv += 2;

	if (argc < 1) {
		fprintf(stderr, "Missing argument (device name)\n");
		return 2;
	}

	ret = do_get_drv_info(argv[0], &chip_id);
	if (!ret)
		printf("%08X\n", chip_id);

	return ret;
}

COMMAND(get, hw_version, "<device name>", 0, 0, CIB_NONE, get_hw_version,
	"Get HW version (chip id)");

