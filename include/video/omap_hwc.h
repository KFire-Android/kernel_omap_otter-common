#ifndef _LINUX_OMAP_HWC_H
#define _LINUX_OMAP_HWC_H

/*
 * Client of the omap_hwc_data API
 * -------------------------------
 * This is really a kernel to kernel API (not an ioctl interface) but most of
 * the values are populated in user space by a single client, specifically the
 * Android hardware composer HAL (HWC) which is a library loaded by the
 * SurfaceFlinger composition process.
 *
 * Implementation details
 * ----------------------
 * The SGX display driver (called OMAPLFB) will obtain this data structure
 * from the GPU driver (aka PVR kernel services) as well as an array of
 * memory descriptors managed by PVR services. OMAPLFB is responsible
 * for fixing up the buffer data within the omap_hwc_data data structure
 * before calling the DSSCOMP module and (optionally) the Bltsville kernel
 * API/implementation.
 *
 * Buffer referencing
 * ------------------
 * When calling from the HWC (via PVR services) the actual address of buffers
 * involved in composition are typically unknown. It is a convention to store
 * an index in the appropriate field of the sub-structures in omap_hwc_data.
 * PVR services also passes an array of memory data structures that these
 * indexes (commonly) refer to.
 *
 * The dsscomp API fully describes how it manages buffering when called
 * from a kernel client context. The simplest scenario is that individual
 * overlays carry the buffer index in the overlay structure base address.
 *
 * For Bltsville kernel mode support, buffers are identified by storing the
 * buffer index in the virtaddr of the various bvbuffdesc entries. Some
 * exceptions are described below.
 *
 * Blitting
 * --------
 * Blit source buffers are managed in the same way as Post2 layers in the
 * dsscomp API. The Android HWC constructs the blits using the Bltsville
 * API but the actual blits are issued to Bltsville kernel mode implementation
 * from OMAPLFB. Destination buffers are allocated and managed by OMAPLFB.
 *
 * When blitting it is expected that HWC will construct the overlay information
 * appropriately in "dsscomp_data"
 */

/*
 * Alternate blit descriptor information.
 *
 * As mentioned above buffers are refers to by index, alternate buffers
 * can be refered to by the following
 */
#define HWC_BLT_DESC_FLAG 0x80000000

/*
 * With this value the descriptor refers to an available framebuffer. It
 * is up to the client and OMAPLFB to track and manage the state of these
 * buffers. The caller can also specify an overlay index that OMAPLFB will
 * use when programming dsscomp with the result of the blit.
 */
#define HWC_BLT_DESC_FB   0x40000000

#define HWC_BLT_DESC_FB_FN(ovlno) \
	(HWC_BLT_DESC_FLAG | HWC_BLT_DESC_FB | (ovlno))

/*
 * This flag hints OMAPLFB the HWC has configured a DSS pipe for a blit FB
 */
#define HWC_BLT_FLAG_USE_FB (1 << 0)

/*****************************************************************************/
/* WARNING - These structs must keep in sync with user space code            */
/*****************************************************************************/
struct rgz_blt_entry {
	struct bvbltparams bp;
	struct bvsurfgeom dstgeom;
	struct bvsurfgeom src1geom;
	struct bvbuffdesc src1desc;
	struct bvsurfgeom src2geom;
	struct bvbuffdesc src2desc;
};

struct omap_hwc_blit_data {
	__u16 rgz_flags;
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
