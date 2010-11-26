/*
 * Defines for zoom boards
 */
#include <plat/display.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

struct flash_partitions {
	struct mtd_partition *parts;
	int nr_parts;
};

extern int __init zoom_debugboard_init(void);
extern void __init zoom_peripherals_init(void);
extern void __init zoom_display_init(enum omap_dss_venc_type venc_type);
extern void __init zoom_flash_init(struct flash_partitions [], int);

#define ZOOM2_HEADSET_EXTMUTE_GPIO	153
#define ZOOM_NAND_CS    0
