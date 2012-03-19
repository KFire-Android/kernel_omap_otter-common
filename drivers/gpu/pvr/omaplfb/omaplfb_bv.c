/*
 * BltsVille support in omaplfb
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/notifier.h>

#include <linux/ion.h>
#include <linux/omap_ion.h>
#include <mach/tiler.h>
#include <linux/gcbv-iface.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "omaplfb.h"

static int debugbv;
extern struct ion_client *gpsIONClient;

/*
 * Are the blit framebuffers in VRAM?
 */
#define OMAPLFB_BLT_FBS_VRAM 1

#define OMAPLFB_COMMAND_COUNT		1

#define	OMAPLFB_VSYNC_SETTLE_COUNT	5

#define	OMAPLFB_MAX_NUM_DEVICES		FB_MAX
#if (OMAPLFB_MAX_NUM_DEVICES > FB_MAX)
#error "OMAPLFB_MAX_NUM_DEVICES must not be greater than FB_MAX"
#endif

static IMG_BOOL gbBvInterfacePresent;
static struct bventry gsBvInterface = {NULL, NULL, NULL};

IMG_BOOL OMAPLFBBltFbClearWorkaround(OMAPLFB_DEVINFO *psDevInfo)
{
	/* Work-around: Map VRAM to make a blit to clear the FB, VRAM has black
	 * transparent pixels only possible if FB is 2D otherwise it won't
	 * work!
	 */

	struct bventry *bv_entry = &gsBvInterface;
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	struct fb_info *psLINFBInfo = psDevInfo->psLINFBInfo;
	unsigned long phy_addr, phy_base, phy_sz;
	unsigned int num_pages;
	unsigned long *page_addrs;
	enum bverror bv_error;
	int j;
	struct bvbuffdesc *buffdesc;
	struct bvsurfgeom *clr_fb_geom;
	IMG_UINT uiBltFBSize = psDevInfo->sFBInfo.ulHeight * psDevInfo->psLINFBInfo->fix.line_length;

	phy_base = phy_addr = psLINFBInfo->fix.smem_start;
	if (!psPVRFBInfo->bIs2D)
	{
		phy_addr += psDevInfo->sFBInfo.ulRoundedBufferSize *
			    psDevInfo->psSwapChain->ulBufferCount;
	}
	phy_addr += (OMAPLFB_BLT_FBS_VRAM) ?
			psPVRFBInfo->psBltFBsNo * uiBltFBSize : 0;

	phy_sz = (psLINFBInfo->fix.line_length * psLINFBInfo->var.yres) + PAGE_SIZE - 1;
	num_pages = phy_sz >> PAGE_SHIFT;
	if ((phy_addr + phy_sz) > (phy_base + psLINFBInfo->fix.smem_len))
	{
		WARN(1, "%s: Blt clear area exceeds VRAM, off %ld sz %ld\n",
			__func__, phy_addr - phy_base, phy_sz);
		return IMG_FALSE;
	}

	page_addrs = kzalloc(sizeof(*page_addrs) *
			num_pages, GFP_KERNEL);
	if (!page_addrs) {
		WARN(1, "%s: Out of memory\n", __func__);
		return IMG_FALSE;
	}

	buffdesc = kzalloc(sizeof(*buffdesc), GFP_KERNEL);
	if (!buffdesc) {
		WARN(1, "%s: Out of memory\n", __func__);
		kfree(page_addrs);
		return IMG_FALSE;
	}

	clr_fb_geom = kzalloc(sizeof(*clr_fb_geom), GFP_KERNEL);
	if (!clr_fb_geom) {
		WARN(1, "%s: Out of memory\n", __func__);
		kfree(buffdesc);
		kfree(page_addrs);
		return IMG_FALSE;
	}

	for(j = 0; j < num_pages; j++) {
		page_addrs[j] = phy_addr + (j * PAGE_SIZE);
	}

	buffdesc->structsize = sizeof(*buffdesc);
	buffdesc->pagesize = PAGE_SIZE;
	buffdesc->pagearray = page_addrs;
	buffdesc->pagecount = num_pages;
	/* Mark the buffer with != 0, otherwise GC driver blows up */
	buffdesc->virtaddr = (void*)10;

	bv_error = bv_entry->bv_map(buffdesc);
	if (bv_error) {
		WARN(1, "%s: BV map swapchain buffer failed %d\n",
				__func__, bv_error);
		psPVRFBInfo->clr_fb_desc = NULL;
		kfree(buffdesc);
		kfree(clr_fb_geom);
		kfree(page_addrs);
		return IMG_FALSE;
	} else
		psPVRFBInfo->clr_fb_desc = buffdesc;

	kfree(page_addrs);

	clr_fb_geom->structsize = sizeof(struct bvsurfgeom);
	clr_fb_geom->format = OCDFMT_BGRA24;
	clr_fb_geom->width = psLINFBInfo->var.xres;
	clr_fb_geom->height = psLINFBInfo->var.yres;
	clr_fb_geom->orientation = 0;
	clr_fb_geom->virtstride = psLINFBInfo->fix.line_length;
	clr_fb_geom->physstride = clr_fb_geom->virtstride;
	psPVRFBInfo->clr_fb_geom = clr_fb_geom;
	return IMG_TRUE;
}

/* FIXME: Manual clipping, this should be done in bltsville not here */
static void clip_rects(struct bvbltparams *pParmsIn)
{
	if (pParmsIn->dstrect.left < 0 || pParmsIn->dstrect.top < 0 ||
		(pParmsIn->dstrect.left + pParmsIn->dstrect.width) >
			pParmsIn->dstgeom->width ||
		(pParmsIn->dstrect.top + pParmsIn->dstrect.height) >
			pParmsIn->dstgeom->height) {

		/* Adjust src1 */
		if (pParmsIn->dstrect.left < 0) {
			pParmsIn->src1rect.left = abs(pParmsIn->dstrect.left);
			pParmsIn->src1rect.width -= pParmsIn->src1rect.left;
			pParmsIn->dstrect.left = 0;
		}

		if (pParmsIn->dstrect.top < 0) {
			pParmsIn->src1rect.top = abs(pParmsIn->dstrect.top);
			pParmsIn->src1rect.height -= pParmsIn->src1rect.top;
			pParmsIn->dstrect.top = 0;
		}

		if (pParmsIn->dstrect.left + pParmsIn->dstrect.width >
			pParmsIn->dstgeom->width) {
			pParmsIn->dstrect.width -= (pParmsIn->dstrect.left +
				pParmsIn->dstrect.width) -
				pParmsIn->dstgeom->width;
			pParmsIn->src1rect.width = pParmsIn->dstrect.width;
		}

		if (pParmsIn->dstrect.top + pParmsIn->dstrect.height >
			pParmsIn->dstgeom->height) {
			pParmsIn->dstrect.height -= (pParmsIn->dstrect.top +
				pParmsIn->dstrect.height) -
				pParmsIn->dstgeom->height;
			pParmsIn->src1rect.height = pParmsIn->dstrect.height;
		}

		/* Adjust src2 */
		if (pParmsIn->dstrect.left < 0) {
			pParmsIn->src2rect.left = abs(pParmsIn->dstrect.left);
			pParmsIn->src2rect.width -= pParmsIn->src2rect.left;
			pParmsIn->dstrect.left = 0;
		}

		if (pParmsIn->dstrect.top < 0) {
			pParmsIn->src2rect.top = abs(pParmsIn->dstrect.top);
			pParmsIn->src2rect.height -= pParmsIn->src2rect.top;
			pParmsIn->dstrect.top = 0;
		}

		if (pParmsIn->dstrect.left + pParmsIn->dstrect.width >
			pParmsIn->dstgeom->width) {
			pParmsIn->dstrect.width -= (pParmsIn->dstrect.left +
				pParmsIn->dstrect.width) -
				pParmsIn->dstgeom->width;
			pParmsIn->src2rect.width = pParmsIn->dstrect.width;
		}

		if (pParmsIn->dstrect.top + pParmsIn->dstrect.height >
			pParmsIn->dstgeom->height) {
			pParmsIn->dstrect.height -= (pParmsIn->dstrect.top +
				pParmsIn->dstrect.height) -
				pParmsIn->dstgeom->height;
			pParmsIn->src2rect.height = pParmsIn->dstrect.height;
		}
	}
}

static void print_bvparams(struct bvbltparams *bltparams,
                           unsigned int pSrc1DescInfo, unsigned int pSrc2DescInfo)
{
	if (bltparams->flags & BVFLAG_BLEND)
	{
		printk(KERN_INFO "%s: param %s %x (%s), flags %ld\n",
			"bv", "blend", bltparams->op.blend,
			bltparams->op.blend == BVBLEND_SRC1OVER ? "src1over" : "??",
			bltparams->flags);
	}

	if (bltparams->flags & BVFLAG_ROP)
	{
		printk(KERN_INFO "%s: param %s %x (%s), flags %ld\n",
			"bv", "rop", bltparams->op.rop,
			bltparams->op.rop == 0xCCCC ? "srccopy" : "??",
			bltparams->flags);
	}

	printk(KERN_INFO "%s: dst %d,%d rect{%d,%d sz %d,%d}"
		" stride %ld desc 0x%p\n", "bv",
		bltparams->dstgeom->width,
		bltparams->dstgeom->height,
		bltparams->dstrect.left, bltparams->dstrect.top,
		bltparams->dstrect.width, bltparams->dstrect.height,
		bltparams->dstgeom->physstride,
		bltparams->dstdesc->pagearray);

	printk(KERN_INFO "%s: src1 %d,%d rect{%d,%d sz %d,%d}"
		" stride %ld, virtaddr 0x%x (0x%x)\n", "bv",
		bltparams->src1geom->width,
		bltparams->src1geom->height, bltparams->src1rect.left,
		bltparams->src1rect.top, bltparams->src1rect.width,
		bltparams->src1rect.height, bltparams->src1geom->physstride,
		(unsigned int)bltparams->src1.desc->virtaddr, pSrc1DescInfo);

	if (!(bltparams->flags & BVFLAG_BLEND))
		return;

	printk(KERN_INFO "%s: src2 %d,%d rect{%d,%d sz %d,%d}"
		" stride %ld, virtaddr 0x%x (0x%x)\n", "bv",
		bltparams->src2geom->width,
		bltparams->src2geom->height, bltparams->src2rect.left,
		bltparams->src2rect.top, bltparams->src2rect.width,
		bltparams->src2rect.height, bltparams->src2geom->physstride,
		(unsigned int)bltparams->src2.desc->virtaddr, pSrc2DescInfo);
}

static enum bverror bv_map_meminfo(OMAPLFB_DEVINFO *psDevInfo,
	struct bventry *bv_entry, struct bvbuffdesc *buffdesc,
	PDC_MEM_INFO *meminfo)
{
	IMG_CPU_PHYADDR phyAddr;
	IMG_UINT32 ui32NumPages;
	IMG_SIZE_T uByteSize;
	unsigned long *page_addrs;
	enum bverror bv_error;
	int i;

	psDevInfo->sPVRJTable.pfnPVRSRVDCMemInfoGetByteSize(*meminfo,
		&uByteSize);
	ui32NumPages = (uByteSize + PAGE_SIZE - 1) >> PAGE_SHIFT;
	page_addrs = kzalloc(sizeof(*page_addrs) * ui32NumPages, GFP_KERNEL);
	if (!page_addrs) {
		WARN(1, "%s: Out of memory\n", __func__);
		return BVERR_OOM;
	}

	for (i = 0; i < ui32NumPages; i++) {
		psDevInfo->sPVRJTable.pfnPVRSRVDCMemInfoGetCpuPAddr(
			*meminfo, i << PAGE_SHIFT, &phyAddr);
		page_addrs[i] = (u32)phyAddr.uiAddr;
	}

	/* Assume the structsize and length is already assigned */
	buffdesc->map = NULL;
	buffdesc->pagesize = PAGE_SIZE;
	buffdesc->pagearray = page_addrs;
	buffdesc->pagecount = ui32NumPages;
	buffdesc->pageoffset = 0;

	bv_error = bv_entry->bv_map(buffdesc);

	kfree(page_addrs);
	return bv_error;
}

void OMAPLFBGetBltFBsBvHndl(OMAPLFB_FBINFO *psPVRFBInfo, IMG_UINTPTR_T *ppPhysAddr)
{
	if (++psPVRFBInfo->iBltFBsIdx >= OMAPLFB_NUM_BLT_FBS)
	{
		psPVRFBInfo->iBltFBsIdx = 0;
	}
	*ppPhysAddr = psPVRFBInfo->psBltFBsBvPhys[psPVRFBInfo->iBltFBsIdx];
}

static OMAPLFB_ERROR InitBltFBsCommon(OMAPLFB_DEVINFO *psDevInfo)
{
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	IMG_INT n = OMAPLFB_NUM_SGX_FBS;

	psPVRFBInfo->psBltFBsNo = n;
	psPVRFBInfo->psBltFBsIonHndl = NULL;
	psPVRFBInfo->psBltFBsBvHndl = kzalloc(n * sizeof(*psPVRFBInfo->psBltFBsBvHndl), GFP_KERNEL);
	if (!psPVRFBInfo->psBltFBsBvHndl)
	{
		return OMAPLFB_ERROR_INIT_FAILURE;
	}

	psPVRFBInfo->psBltFBsBvPhys = kzalloc(n * sizeof(*psPVRFBInfo->psBltFBsBvPhys), GFP_KERNEL);
	if (!psPVRFBInfo->psBltFBsBvPhys)
	{
		return OMAPLFB_ERROR_INIT_FAILURE;
	}
	/* Freeing of resources is handled in deinit code */
	return OMAPLFB_OK;
}

/*
 * Initialize the blit framebuffers and create the Bltsville mappings, these
 * buffers are separate from the swapchain to reduce complexity
 */
static OMAPLFB_ERROR InitBltFBsVram(OMAPLFB_DEVINFO *psDevInfo)
{
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	IMG_UINT uiBltFBSize = psDevInfo->sFBInfo.ulHeight * psDevInfo->psLINFBInfo->fix.line_length;
	IMG_UINT uiNumPages = uiBltFBSize >> PAGE_SHIFT;
	IMG_UINT uiFb;

	if (InitBltFBsCommon(psDevInfo) != OMAPLFB_OK)
	{
		return OMAPLFB_ERROR_INIT_FAILURE;
	}
	psPVRFBInfo->uiBltFBsByteStride = psDevInfo->psLINFBInfo->fix.line_length;

	for (uiFb = 0; uiFb < psPVRFBInfo->psBltFBsNo; uiFb++)
	{
		unsigned long *pPaddrs;
		enum bverror iBvErr;
		IMG_UINT j;
		struct bvbuffdesc *pBvDesc;
		IMG_UINT uiVramStart;
		IMG_UINT uiFbOff;

		pPaddrs = kzalloc(sizeof(*pPaddrs) *
				uiNumPages, GFP_KERNEL);
		if (!pPaddrs)
		{
			return OMAPLFB_ERROR_OUT_OF_MEMORY;
		}

		pBvDesc = kzalloc(sizeof(*pBvDesc), GFP_KERNEL);
		if (!pBvDesc)
		{
			kfree(pPaddrs);
			return OMAPLFB_ERROR_OUT_OF_MEMORY;
		}
		/*
		 * Handle the swapchain buffers being located in TILER2D or in
		 * VRAM
		 */
		uiFbOff = psPVRFBInfo->bIs2D ? 0 :
					psDevInfo->sFBInfo.ulRoundedBufferSize *
					psDevInfo->psSwapChain->ulBufferCount;
		uiVramStart = psDevInfo->psLINFBInfo->fix.smem_start + uiFbOff +
					(uiBltFBSize * uiFb);

		for(j = 0; j < uiNumPages; j++)
		{
			pPaddrs[j] = uiVramStart + (j * PAGE_SIZE);
		}
		psPVRFBInfo->psBltFBsBvPhys[uiFb] = pPaddrs[0];

		pBvDesc->structsize = sizeof(*pBvDesc);
		pBvDesc->pagesize = PAGE_SIZE;
		pBvDesc->pagearray = pPaddrs;
		pBvDesc->pagecount = uiNumPages;
		pBvDesc->length = pBvDesc->pagecount * pBvDesc->pagesize;
		/* Mark the buffer with != 0, otherwise GC driver blows up */
		pBvDesc->virtaddr = (void*)0xFACEC0D;


		iBvErr = gsBvInterface.bv_map(pBvDesc);
		if (iBvErr)
		{
			WARN(1, "%s: BV map Blt FB buffer failed %d\n",
					__func__, iBvErr);
			kfree(pBvDesc);
			kfree(pPaddrs);
			return OMAPLFB_ERROR_GENERIC;
		}
		psPVRFBInfo->psBltFBsBvHndl[uiFb] = pBvDesc;
		kfree(pPaddrs);
	}
	return OMAPLFB_OK;
}

static PVRSRV_ERROR InitBltFBsMapTiler2D(OMAPLFB_DEVINFO *psDevInfo)
{
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	struct fb_info *psLINFBInfo = psDevInfo->psLINFBInfo;
	struct bvbuffdesc *pBvDesc;
	struct bventry *pBvEntry;
	enum bverror eBvErr;
	int iFB;
	ion_phys_addr_t phys;
	size_t size;
	int res = PVRSRV_OK;

	pBvEntry = &gsBvInterface;
	ion_phys(gpsIONClient, psPVRFBInfo->psBltFBsIonHndl, &phys, &size);

	for (iFB = 0; iFB < psPVRFBInfo->psBltFBsNo; iFB++)
	{
		unsigned long *pPageList;

		struct tiler_view_t view;
		int wpages = psPVRFBInfo->uiBltFBsByteStride >> PAGE_SHIFT;
		int h = psLINFBInfo->var.yres;
		int x, y;

		phys += psPVRFBInfo->uiBltFBsByteStride * iFB;
		pPageList = kzalloc(
				wpages * h * sizeof(*pPageList),
		                GFP_KERNEL);
		if ( !pPageList) {
			printk(KERN_WARNING DRIVER_PREFIX
					": %s: Could not allocate page list\n",
					__FUNCTION__);
			return OMAPLFB_ERROR_INIT_FAILURE;
		}
		tilview_create(&view, phys, psLINFBInfo->var.xres, h);
		for (y = 0; y < h; y++) {
			for (x = 0; x < wpages; x++) {
				pPageList[y * wpages + x] = phys + view.v_inc * y
						+ (x << PAGE_SHIFT);
			}
		}
		pBvDesc = kzalloc(sizeof(*pBvDesc), GFP_KERNEL);
		pBvDesc->structsize = sizeof(*pBvDesc);
		pBvDesc->pagesize = PAGE_SIZE;
		pBvDesc->pagearray = pPageList;
		pBvDesc->pagecount = wpages * h;

		eBvErr = pBvEntry->bv_map(pBvDesc);

		pBvDesc->pagearray = NULL;

		if (eBvErr)
		{
			WARN(1, "%s: BV map blt buffer failed %d\n",__func__, eBvErr);
			psPVRFBInfo->psBltFBsBvHndl[iFB]= NULL;
			kfree(pBvDesc);
			res = PVRSRV_ERROR_OUT_OF_MEMORY;
		}
		else
		{
			psPVRFBInfo->psBltFBsBvHndl[iFB] = pBvDesc;
			psPVRFBInfo->psBltFBsBvPhys[iFB] = pPageList[0];
		}
		kfree(pPageList);
	}

	return res;
}

/*
 * Allocate buffers from the blit 'framebuffers'. These buffers are not shared
 * with the SGX flip chain to reduce complexity
 */
static OMAPLFB_ERROR InitBltFBsTiler2D(OMAPLFB_DEVINFO *psDevInfo)
{
	/*
	 * Pick up the calculated bytes per pixel from the deduced
	 * OMAPLFB_FBINFO, get the rest of the display parameters from the
	 * struct fb_info
	 */
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	struct fb_info *psLINFBInfo = psDevInfo->psLINFBInfo;
	int res, w, h;

	struct omap_ion_tiler_alloc_data sAllocData = {
		.fmt = psPVRFBInfo->uiBytesPerPixel == 2 ? TILER_PIXEL_FMT_16BIT : TILER_PIXEL_FMT_32BIT,
		.flags = 0,
	};

	if (InitBltFBsCommon(psDevInfo) != OMAPLFB_OK)
	{
		return OMAPLFB_ERROR_INIT_FAILURE;
	}
	psPVRFBInfo->uiBltFBsByteStride = PAGE_ALIGN(psLINFBInfo->var.xres * psPVRFBInfo->uiBytesPerPixel);

	/* TILER will align width to 128-bytes */
	/* however, SGX must have full page width */
	w = ALIGN(psLINFBInfo->var.xres, PAGE_SIZE / psPVRFBInfo->uiBytesPerPixel);
	h = psLINFBInfo->var.yres;
	sAllocData.h = h;
	sAllocData.w = psPVRFBInfo->psBltFBsNo * w;

	printk(KERN_INFO DRIVER_PREFIX
		":BltFBs alloc %d x (%d x %d) [stride %d]\n",
		psPVRFBInfo->psBltFBsNo, w, h, psPVRFBInfo->uiBltFBsByteStride);
	res = omap_ion_nonsecure_tiler_alloc(gpsIONClient, &sAllocData);
	if (res < 0)
	{
		printk(KERN_ERR DRIVER_PREFIX
			"Could not allocate BltFBs\n");
		return OMAPLFB_ERROR_INIT_FAILURE;
	}

	psPVRFBInfo->psBltFBsIonHndl = sAllocData.handle;

	res = InitBltFBsMapTiler2D(psDevInfo);
	if (res != OMAPLFB_OK)
	{
		return OMAPLFB_ERROR_INIT_FAILURE;
	}
	return OMAPLFB_OK;
}

void OMAPLFBDoBlits(OMAPLFB_DEVINFO *psDevInfo, PDC_MEM_INFO *ppsMemInfos, struct omap_hwc_blit_data *blit_data, IMG_UINT32 ui32NumMemInfos)
{
	struct rgz_blt_entry *entry_list;
	struct bventry *bv_entry = &gsBvInterface;
	struct bvbuffdesc src1desc;
	struct bvbuffdesc src2desc;
	struct bvbuffdesc *dstdesc = NULL;
	struct bvsurfgeom src1geom;
	struct bvsurfgeom src2geom;
	struct bvsurfgeom dstgeom;
	struct bvbltparams bltparams;
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	int rgz_items = blit_data->rgz_items;
	int j;

	dstdesc = psPVRFBInfo->psBltFBsBvHndl[psPVRFBInfo->iBltFBsIdx];

	/* DSS pipes are setup up to this point, we can begin blitting here */
	entry_list = (struct rgz_blt_entry *) (blit_data->rgz_blts);
	for (j = 0; j < rgz_items; j++)
	{
		struct rgz_blt_entry *entry = &entry_list[j];
		enum bverror bv_error = 0;
		unsigned int meminfo_ix;
		int src1_mapped = 0, src2_mapped = 0;
		unsigned int iSrc1DescInfo = 0, iSrc2DescInfo = 0;
		if (debugbv)
		{
			iSrc1DescInfo = (unsigned int)entry->src1desc.virtaddr;
			iSrc2DescInfo = (unsigned int)entry->src2desc.virtaddr;
		}

		/* BV Parameters data */
		bltparams = entry->bp;

		/* Src1 buffer data */
		src1desc = entry->src1desc;
		src1geom = entry->src1geom;
		src1geom.physstride = src1geom.virtstride;

		meminfo_ix = (unsigned int)src1desc.virtaddr;
		/* Making fill, avoid mapping src1 */
		/* This will change when the HWC starts to use HWC_BLT_FLAG_CLR */
		if (meminfo_ix == -1)
		{
#if 0
			/* FIXME: Doesn't work! use below alternative */
			/* Clean FB with GC 320 with black transparent pixel */
			static unsigned int pixel = 0;
			src1desc->virtaddr = (void*)&pixel;
#else
			/* Use pre-mappped VRAM full of transparent pixels */
			dstgeom = entry->dstgeom;
			bltparams.src1rect.width = dstgeom.width;
			bltparams.src1rect.height = dstgeom.height;
			src1desc = *(struct bvbuffdesc *)psDevInfo->sFBInfo.clr_fb_desc;
			src1geom = *((struct bvsurfgeom *)psDevInfo->sFBInfo.clr_fb_geom);
#endif
		}
		else if (meminfo_ix & HWC_BLT_DESC_FLAG)
		{
			/* This code might be redundant? Do we want to ever blit out of the framebuffer? */
			if (meminfo_ix & HWC_BLT_DESC_FB)
			{
				src1desc = *dstdesc;
			}
		}
		else
		{
			if (!meminfo_idx_valid(meminfo_ix, ui32NumMemInfos))
				continue;

			bv_error = bv_map_meminfo(psDevInfo, bv_entry, &src1desc,
					&ppsMemInfos[meminfo_ix]);
			if (bv_error) {
				WARN(1, "%s: BV map src1 failed %d\n",
						__func__, bv_error);
				continue;
			}
			src1_mapped = 1;
			/* FIXME: Not sure why the virtaddr needs to be unique
			 * for the buffers involved in the blit, if the
			 * following is not done the blit fails, BV GC driver
			 * issue?
			 */
			src1desc.virtaddr = (void*)0xBADFACE;
		}

		/* Dst buffer data, assume meminfo 0 is the FB */
		dstgeom = entry->dstgeom;
		dstgeom.virtstride = dstgeom.physstride = psDevInfo->sFBInfo.uiBltFBsByteStride;

		/* Src2 buffer data
		 * Check if this blit involves src2 as the FB or another
		 * buffer, if the last case is true then map the src2 buffer
		 */
		if (bltparams.flags & BVFLAG_BLEND) {
			src2desc = entry->src2desc;
			meminfo_ix = (unsigned int)src2desc.virtaddr;
			if (meminfo_ix & HWC_BLT_DESC_FLAG)
			{
				if (meminfo_ix & HWC_BLT_DESC_FB)
				{
					/* Blending with destination (FB) */
					src2desc = *dstdesc;
					src2geom = dstgeom; /* Why XXX ?? */
					src2_mapped = 0;
				}
			}
			else {
				/* Blending with other buffer */

				src2geom = entry->src2geom;
				src2geom.physstride = src2geom.virtstride;

				if (!meminfo_idx_valid(meminfo_ix,
						ui32NumMemInfos))
					goto unmap_srcs;

				bv_error = bv_map_meminfo(psDevInfo, bv_entry,
						&src2desc, &ppsMemInfos[meminfo_ix]);
				if (bv_error) {
					WARN(1, "%s: BV map dst failed %d\n",
							__func__, bv_error);
					goto unmap_srcs;
				}
				src2_mapped = 1;
				/* FIXME: Not sure why the virtaddr needs to be
				 * unique for the buffers involved in the blit,
				 * if the following is not done the blit fails,
				 * BV GC driver issue?
				 */
				src2desc.virtaddr = (void *)0x00C0FFEE;
			}
		}

		bltparams.dstdesc = dstdesc;
		bltparams.dstgeom = &dstgeom;
		bltparams.src1.desc = &src1desc;
		bltparams.src1geom = &src1geom;
		bltparams.src2.desc = &src2desc;
		bltparams.src2geom = &src2geom;

		/* FIXME: BV GC2D clipping support is not done properly,
		 * clip manually while this is fixed
		 */
		clip_rects(&bltparams);

		if (debugbv)
		{
			print_bvparams(&bltparams, iSrc1DescInfo, iSrc2DescInfo);
		}

		bv_error = bv_entry->bv_blt(&bltparams);
		if (bv_error)
			printk(KERN_ERR "%s: blit failed %d\n",
					__func__, bv_error);
		unmap_srcs:
		if (src1_mapped)
			bv_entry->bv_unmap(&src1desc);
		if (src2_mapped)
			bv_entry->bv_unmap(&src2desc);
	}
}

OMAPLFB_ERROR OMAPLFBInitBltFBs(OMAPLFB_DEVINFO *psDevInfo)
{
	return (OMAPLFB_BLT_FBS_VRAM) ? InitBltFBsVram(psDevInfo) : InitBltFBsTiler2D(psDevInfo);
}

void OMAPLFBDeInitBltFBs(OMAPLFB_DEVINFO *psDevInfo)
{
	struct bventry *bv_entry = &gsBvInterface;
	OMAPLFB_FBINFO *psPVRFBInfo = &psDevInfo->sFBInfo;
	struct bvbuffdesc *pBufDesc;

	if (!gbBvInterfacePresent)
	{
		return;
	}

	if (psPVRFBInfo->psBltFBsBvHndl)
	{
		IMG_INT i;
		for (i = 0; i < psPVRFBInfo->psBltFBsNo; i++)
		{
			pBufDesc = psPVRFBInfo->psBltFBsBvHndl[i];
			if (pBufDesc)
			{
				bv_entry->bv_unmap(pBufDesc);
				kfree(pBufDesc);
			}
		}
	}
	kfree(psPVRFBInfo->psBltFBsBvHndl);

	if (psPVRFBInfo->psBltFBsIonHndl)
	{
		ion_free(gpsIONClient, psPVRFBInfo->psBltFBsIonHndl);
	}

	pBufDesc = psPVRFBInfo->clr_fb_desc;
	if (pBufDesc)
	{
		bv_entry->bv_unmap(pBufDesc);
		kfree(pBufDesc);
	}

	if (psPVRFBInfo->clr_fb_geom)
	{
		kfree(psPVRFBInfo->clr_fb_geom);
	}
}

IMG_BOOL OMAPLFBInitBlt(void)
{
#if defined(CONFIG_GCBV)
	/* Get the GC2D Bltsville implementation */
	gcbv_init(&gsBvInterface);
	gbBvInterfacePresent = gsBvInterface.bv_map ? IMG_TRUE : IMG_FALSE;
#else
	gbBvInterfacePresent = IMG_FALSE;
#endif
	return gbBvInterfacePresent;
}
