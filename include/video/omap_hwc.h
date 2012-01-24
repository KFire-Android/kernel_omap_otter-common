#ifndef _LINUX_OMAP_HWC_H
#define _LINUX_OMAP_HWC_H

struct rgz_blt_entry {
	struct bvbltparams bp;
	struct bvsurfgeom dstgeom;
	struct bvsurfgeom src1geom;
	struct bvbuffdesc src1desc;
	struct bvsurfgeom src2geom;
	struct bvbuffdesc src2desc;
};

struct omap_hwc_blit_data {
	/* if rgz_items is 0 there is nothing to blit */
	__u16 rgz_items;
	struct rgz_blt_entry rgz_blts[0];
};

/*
 * This structure is passed down from the Android HWC HAL
 */
struct omap_hwc_data {
	struct dsscomp_setup_dispc_data dsscomp_data;
	struct omap_hwc_blit_data blit_data;
};

#endif
