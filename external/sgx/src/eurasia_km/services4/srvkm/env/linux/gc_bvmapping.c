/*
 * Copyright (C) 2011 Texas Instruments, Inc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/bltsville.h>
#include <linux/bvinternal.h>
#include <linux/gcbv-iface.h>

#include "gc_bvmapping.h"
#include "services_headers.h"

void gc_bvmap_meminfo(PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	int i;
	IMG_CPU_PHYADDR phy_addr;
	unsigned long *page_addrs;
	struct bvbuffdesc *buffdesc;
	struct bvphysdesc *physdesc;
	int num_pages;
	struct bventry bv_entry;
	enum bverror bv_error;

	gcbv_init(&bv_entry);
	if (!bv_entry.bv_map) {
		psMemInfo->bvmap_handle = NULL;
		return;
	}

	num_pages = (psMemInfo->uAllocSize +
		PAGE_SIZE - 1) >> PAGE_SHIFT;

	page_addrs = kzalloc(sizeof(*page_addrs) * num_pages, GFP_KERNEL);
	if (!page_addrs) {
		printk(KERN_ERR "%s: Out of memory\n", __func__);
		return;
	}

	physdesc = kzalloc(sizeof(*physdesc), GFP_KERNEL);
	buffdesc = kzalloc(sizeof(*buffdesc), GFP_KERNEL);
	if (!buffdesc || !physdesc) {
		printk(KERN_ERR "%s: Out of memory\n", __func__);
		kfree(page_addrs);
		kfree(physdesc);
		kfree(buffdesc);
		return;
	}

	for (i = 0; i < num_pages; i++) {
		phy_addr = OSMemHandleToCpuPAddr(
			psMemInfo->sMemBlk.hOSMemHandle, i << PAGE_SHIFT);
		page_addrs[i] = (u32)phy_addr.uiAddr;
	}

	buffdesc->structsize = sizeof(*buffdesc);
	buffdesc->map = NULL;
	buffdesc->length = psMemInfo->uAllocSize;
	buffdesc->auxtype = BVAT_PHYSDESC;
	buffdesc->auxptr = physdesc;
	physdesc->structsize = sizeof(*physdesc);
	physdesc->pagesize = PAGE_SIZE;
	physdesc->pagearray = page_addrs;
	physdesc->pagecount = num_pages;

	/*
	 * For ion allocated buffers let's verify how many planes this
	 * meminfo consist of
	 */
	if(psMemInfo->ui32Flags & PVRSRV_MEM_ION) {
		IMG_UINT32 num_addr_offsets = 0;
		OSGetMemMultiPlaneInfo(psMemInfo->sMemBlk.hOSMemHandle,
			NULL, &num_addr_offsets);

		/*
		 * Account for this meminfo plane offset (relative to the base
		 * address) if necessary
		 */
		if(num_addr_offsets > 0)
			physdesc->pageoffset = psMemInfo->planeOffsets[0];

		/*
		 * In BV there is no way to specify multiple offsets, check
		 * all planes have the same offset and report any discrepancy
		 */
		for (i = 1; i < num_addr_offsets; i++) {
			IMG_UINT32 plane_offset =
				psMemInfo->planeOffsets[i] % PAGE_SIZE;
			if (psMemInfo->planeOffsets[0] != plane_offset) {
				printk(KERN_WARNING "%s: meminfo %p offset 0 %d"
					" != offset %d %d, coalignment is "
					"missing\n", __func__, psMemInfo,
					psMemInfo->planeOffsets[0],
					i, plane_offset);
			}
		}
	}

	bv_error = bv_entry.bv_map(buffdesc);
	if (bv_error) {
		printk(KERN_ERR "%s: Failed to map meminfo %p, bverror %d\n",
			__func__, psMemInfo, bv_error);
		psMemInfo->bvmap_handle = NULL;
	} else
		psMemInfo->bvmap_handle = buffdesc;

}

void gc_bvunmap_meminfo(PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	struct bvbuffdesc *buffdesc;
	struct bvphysdesc *physdesc;
	struct bventry bv_entry;
	enum bverror bv_error;

	gcbv_init(&bv_entry);
	if (!bv_entry.bv_map || !psMemInfo || !psMemInfo->bvmap_handle)
		return;

	buffdesc = psMemInfo->bvmap_handle;
	physdesc = (struct bvphysdesc*) buffdesc->auxptr;
	bv_error = bv_entry.bv_unmap(buffdesc);
	if (bv_error) {
		printk(KERN_ERR "%s: Failed to unmap bvhandle %p from meminfo "
			"%p, bverror %d\n", __func__, buffdesc, psMemInfo,
			bv_error);
	}

	kfree(physdesc->pagearray);
	kfree(physdesc);
	kfree(psMemInfo->bvmap_handle);
	psMemInfo->bvmap_handle = NULL;
}

IMG_VOID *gc_meminfo_to_hndl(PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	return psMemInfo->bvmap_handle;
}
