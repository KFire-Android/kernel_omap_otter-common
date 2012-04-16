/*
 * gcmmu.c
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "gcmain.h"

#define GCZONE_ALL		(~0U)
#define GCZONE_MAPPING		(1 << 0)
#define GCZONE_FIXUP		(1 << 1)
#define GCZONE_FLUSH		(1 << 2)

#include <linux/init.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/pagemap.h>
#include <linux/sched.h>

#include <linux/gcx.h>
#include "gcmmu.h"
#include "gccmdbuf.h"

/*
 * Debugging.
 */

#ifndef GC_FLUSH_USER_PAGES
#	define GC_FLUSH_USER_PAGES 0
#endif

#if !defined(PFN_DOWN)
#	define PFN_DOWN(x) \
		((x) >> PAGE_SHIFT)
#endif

#if !defined(phys_to_pfn)
#	define phys_to_pfn(phys) \
		(PFN_DOWN(phys))
#endif

#if !defined(phys_to_page)
#	define phys_to_page(paddr) \
		(pfn_to_page(phys_to_pfn(paddr)))
#endif

#define ARENA_PREALLOC_SIZE	MMU_PAGE_SIZE
#define ARENA_PREALLOC_COUNT \
	((ARENA_PREALLOC_SIZE - sizeof(struct mmu2darenablock)) \
		/ sizeof(struct mmu2darena))

typedef u32 (*pfn_get_present) (u32 entry);
typedef void (*pfn_print_entry) (u32 index, u32 entry);

struct mm2dtable {
	char *name;
	u32 entry_count;
	u32 vacant_entry;
	pfn_get_present get_present;
	pfn_print_entry print_entry;
};

static inline struct mmu2dprivate *get_mmu(void)
{
	static struct mmu2dprivate _mmu;
	return &_mmu;
}

/*
 * Arena record management.
 */

static enum gcerror mmu2d_get_arena(struct mmu2dprivate *mmu,
					struct mmu2darena **arena)
{
	int i;
	struct mmu2darenablock *block;
	struct mmu2darena *temp;

	if (mmu->arena_recs == NULL) {
		block = kmalloc(ARENA_PREALLOC_SIZE, GFP_KERNEL);
		if (block == NULL)
			return GCERR_SETGRP(GCERR_OODM, GCERR_MMU_ARENA_ALLOC);

		block->next = mmu->arena_blocks;
		mmu->arena_blocks = block;

		temp = (struct mmu2darena *)(block + 1);
		for (i = 0; i < ARENA_PREALLOC_COUNT; i += 1) {
			temp->next = mmu->arena_recs;
			mmu->arena_recs = temp;
			temp += 1;
		}
	}

	*arena = mmu->arena_recs;
	mmu->arena_recs = mmu->arena_recs->next;

	return GCERR_NONE;
}

static void mmu2d_free_arena(struct mmu2dprivate *mmu, struct mmu2darena *arena)
{
	/* Add back to the available arena list. */
	arena->next = mmu->arena_recs;
	mmu->arena_recs = arena;
}

static int mmu2d_siblings(struct mmu2darena *arena1, struct mmu2darena *arena2)
{
	int result;

	if ((arena1 == NULL) || (arena2 == NULL)) {
		result = 0;
	} else {
		u32 mtlb_idx, stlb_idx;
		u32 count, available;

		mtlb_idx = arena1->mtlb;
		stlb_idx = arena1->stlb;
		count = arena1->count;

		while (count > 0) {
			available = MMU_STLB_ENTRY_NUM - stlb_idx;

			if (available > count) {
				available = count;
				stlb_idx += count;
			} else {
				mtlb_idx += 1;
				stlb_idx  = 0;
			}

			count -= available;
		}

		result = (mtlb_idx == arena2->mtlb)
			&& (stlb_idx == arena2->stlb);
	}

	return result;
}

/*
 * Slave table allocation management.
 */

#if MMU_ENABLE
static enum gcerror mmu2d_allocate_slave(struct mmu2dcontext *ctxt,
						struct mmu2dstlb **stlb)
{
	enum gcerror gcerror;
	int i;
	struct mmu2dstlbblock *block;
	struct mmu2dstlb *temp;

	if (ctxt->slave_recs == NULL) {
		block = kmalloc(STLB_PREALLOC_SIZE, GFP_KERNEL);
		if (block == NULL)
			return GCERR_SETGRP(GCERR_OODM, GCERR_MMU_STLB_ALLOC);

		block->next = ctxt->slave_blocks;
		ctxt->slave_blocks = block;

		temp = (struct mmu2dstlb *)(block + 1);
		for (i = 0; i < STLB_PREALLOC_COUNT; i += 1) {
			temp->next = ctxt->slave_recs;
			ctxt->slave_recs = temp;
			temp += 1;
		}
	}

	gcerror = gc_alloc_pages(&ctxt->slave_recs->pages, MMU_STLB_SIZE);
	if (gcerror != GCERR_NONE)
		return GCERR_SETGRP(gcerror, GCERR_MMU_STLB_ALLOC);

	/* Remove from the list of available records. */
	temp = ctxt->slave_recs;
	ctxt->slave_recs = ctxt->slave_recs->next;

	/* Invalidate all entries. */
	for (i = 0; i < MMU_STLB_ENTRY_NUM; i += 1)
		temp->pages.logical[i] = MMU_STLB_ENTRY_VACANT;

	/* Reset allocated entry count. */
	temp->count = 0;

	*stlb = temp;
	return GCERR_NONE;
}
#endif

static enum gcerror virt2phys(u32 logical, pte_t *physical)
{
	pgd_t *pgd;	/* Page Global Directory (PGD). */
	pmd_t *pmd;	/* Page Middle Directory (PMD). */
	pte_t *pte;	/* Page Table Entry (PTE). */

	/* Get the pointer to the entry in PGD for the address. */
	pgd = pgd_offset(current->mm, logical);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return GCERR_MMU_PAGE_BAD;

	/* Get the pointer to the entry in PMD for the address. */
	pmd = pmd_offset(pgd, logical);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return GCERR_MMU_PAGE_BAD;

	/* Get the pointer to the entry in PTE for the address. */
	pte = pte_offset_map(pmd, logical);
	if ((pte == NULL) || !pte_present(*pte))
		return GCERR_MMU_PAGE_BAD;

	*physical = (*pte & PAGE_MASK) | (logical & ~PAGE_MASK);
	return GCERR_NONE;
}

static enum gcerror get_physical_pages(struct mmu2dphysmem *mem,
					pte_t *parray,
					struct mmu2darena *arena)
{
	enum gcerror gcerror = GCERR_NONE;
	struct vm_area_struct *vma;
	struct page **pages = NULL;
	u32 base, write;
	int i, count = 0;

	/* Reset page descriptor array. */
	arena->pages = NULL;

	/* Get base address shortcut. */
	base = mem->base;

	/* Store the logical pointer. */
	arena->logical = (void *) base;

	/*
	 * Important Note: base is mapped from user application process
	 * to current process - it must lie completely within the current
	 * virtual memory address space in order to be of use to us here.
	 */

	vma = find_vma(current->mm, base + (mem->count << PAGE_SHIFT) - 1);
	if ((vma == NULL) || (base < vma->vm_start)) {
		gcerror = GCERR_MMU_BUFFER_BAD;
		goto exit;
	}

	/* Allocate page descriptor array. */
	pages = kmalloc(mem->count * sizeof(struct page *), GFP_KERNEL);
	if (pages == NULL) {
		gcerror = GCERR_SETGRP(GCERR_OODM, GCERR_MMU_DESC_ALLOC);
		goto exit;
	}

	/* Query page descriptors. */
	write = ((vma->vm_flags & (VM_WRITE | VM_MAYWRITE)) != 0) ? 1 : 0;
	count = get_user_pages(current, current->mm, base, mem->count,
				write, 1, pages, NULL);

	if (count < 0) {
		/* Kernel allocated buffer. */
		for (i = 0; i < mem->count; i += 1) {
			gcerror = virt2phys(base, &parray[i]);
			if (gcerror != GCERR_NONE)
				goto exit;

			base += mem->pagesize;
		}
	} else if (count == mem->count) {
		/* User allocated buffer. */
		for (i = 0; i < mem->count; i += 1) {
			parray[i] = page_to_phys(pages[i]);
			if (phys_to_page(parray[i]) != pages[i]) {
				gcerror = GCERR_MMU_PAGE_BAD;
				goto exit;
			}
		}

		/* Set page descriptor array. */
		arena->pages = pages;
	} else {
		gcerror = GCERR_MMU_BUFFER_BAD;
		goto exit;
	}

exit:
	if (arena->pages == NULL) {
		for (i = 0; i < count; i += 1)
			page_cache_release(pages[i]);

		if (pages != NULL)
			kfree(pages);
	}

	return gcerror;
}

static void release_physical_pages(struct mmu2darena *arena)
{
	u32 i;

	if (arena->pages != NULL) {
		for (i = 0; i < arena->count; i += 1)
			page_cache_release(arena->pages[i]);

		kfree(arena->pages);
		arena->pages = NULL;
	}
}

#if GC_FLUSH_USER_PAGES
static void flush_user_buffer(struct mmu2darena *arena)
{
	u32 i;
	struct gcpage gcpage;
	unsigned char *logical;

	if (arena->pages == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"page array is NULL.\n",
			__func__, __LINE__);
		return;
	}


	logical = arena->logical;
	if (logical == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"buffer base is NULL.\n",
			__func__, __LINE__);
		return;
	}

	for (i = 0; i < arena->count; i += 1) {
		gcpage.order = get_order(PAGE_SIZE);
		gcpage.size = PAGE_SIZE;

		gcpage.pages = arena->pages[i];
		if (gcpage.pages == NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"page structure %d is NULL.\n",
				__func__, __LINE__, i);
			continue;
		}

		gcpage.physical = page_to_phys(gcpage.pages);
		if (gcpage.physical == 0) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"physical address of page %d is 0.\n",
				__func__, __LINE__, i);
			continue;
		}

		gcpage.logical = (unsigned int *) (logical + i * PAGE_SIZE);
		if (gcpage.logical == NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"virtual address of page %d is NULL.\n",
				__func__, __LINE__, i);
			continue;
		}

		gc_flush_pages(&gcpage);
	}
}
#endif

enum gcerror mmu2d_create_context(struct mmu2dcontext *ctxt)
{
	enum gcerror gcerror;

#if MMU_ENABLE
	int i;
#endif

	struct mmu2dprivate *mmu = get_mmu();

	if (ctxt == NULL)
		return GCERR_MMU_CTXT_BAD;

	memset(ctxt, 0, sizeof(struct mmu2dcontext));

#if MMU_ENABLE
	/* Allocate MTLB table. */
	gcerror = gc_alloc_pages(&ctxt->master, MMU_MTLB_SIZE);
	if (gcerror != GCERR_NONE) {
		gcerror = GCERR_SETGRP(gcerror, GCERR_MMU_MTLB_ALLOC);
		goto fail;
	}

	/* Allocate an array of pointers to slave descriptors. */
	ctxt->slave = kmalloc(MMU_MTLB_SIZE, GFP_KERNEL);
	if (ctxt->slave == NULL) {
		gcerror = GCERR_SETGRP(GCERR_OODM, GCERR_MMU_STLBIDX_ALLOC);
		goto fail;
	}
	memset(ctxt->slave, 0, MMU_MTLB_SIZE);

	/* Invalidate all entries. */
	for (i = 0; i < MMU_MTLB_ENTRY_NUM; i += 1)
		ctxt->master.logical[i] = MMU_MTLB_ENTRY_VACANT;

	/* Configure the physical address. */
	ctxt->physical
	= SETFIELD(~0U, GCREG_MMU_CONFIGURATION, ADDRESS,
	  (ctxt->master.physical >> GCREG_MMU_CONFIGURATION_ADDRESS_Start))
	& SETFIELDVAL(~0U, GCREG_MMU_CONFIGURATION, MASK_ADDRESS, ENABLED)
	& SETFIELD(~0U, GCREG_MMU_CONFIGURATION, MODE, MMU_MTLB_MODE)
	& SETFIELDVAL(~0U, GCREG_MMU_CONFIGURATION, MASK_MODE, ENABLED);
#endif

	/* Allocate the first vacant arena. */
	gcerror = mmu2d_get_arena(mmu, &ctxt->vacant);
	if (gcerror != GCERR_NONE)
		goto fail;

	/* Everything is vacant. */
	ctxt->vacant->mtlb  = 0;
	ctxt->vacant->stlb  = 0;
	ctxt->vacant->count = MMU_MTLB_ENTRY_NUM * MMU_STLB_ENTRY_NUM;
	ctxt->vacant->next  = NULL;

	/* Nothing is allocated. */
	ctxt->allocated = NULL;

#if MMU_ENABLE
	/* Allocate the safe zone. */
	if (mmu->safezone.size == 0) {
		gcerror = gc_alloc_pages(&mmu->safezone,
						MMU_SAFE_ZONE_SIZE);
		if (gcerror != GCERR_NONE) {
			gcerror = GCERR_SETGRP(gcerror,
						GCERR_MMU_SAFE_ALLOC);
			goto fail;
		}

		/* Initialize safe zone to a value. */
		for (i = 0; i < MMU_SAFE_ZONE_SIZE / sizeof(u32); i += 1)
			mmu->safezone.logical[i] = 0xDEADC0DE;
	}
#endif

	/* Reference MMU. */
	mmu->refcount += 1;
	ctxt->mmu = mmu;

	return GCERR_NONE;

fail:
#if MMU_ENABLE
	gc_free_pages(&ctxt->master);
	if (ctxt->slave != NULL)
		kfree(ctxt->slave);
#endif

	return gcerror;
}

enum gcerror mmu2d_destroy_context(struct mmu2dcontext *ctxt)
{
	int i;
	struct mmu2dstlbblock *nextblock;

	if ((ctxt == NULL) || (ctxt->mmu == NULL))
		return GCERR_MMU_CTXT_BAD;

	if (ctxt->slave != NULL) {
		for (i = 0; i < MMU_MTLB_ENTRY_NUM; i += 1) {
			if (ctxt->slave[i] != NULL) {
				gc_free_pages(&ctxt->slave[i]->pages);
				ctxt->slave[i] = NULL;
			}
		}
		kfree(ctxt->slave);
		ctxt->slave = NULL;
	}

	gc_free_pages(&ctxt->master);

	while (ctxt->slave_blocks != NULL) {
		nextblock = ctxt->slave_blocks->next;
		kfree(ctxt->slave_blocks);
		ctxt->slave_blocks = nextblock;
	}

	ctxt->slave_recs = NULL;

	while (ctxt->allocated != NULL) {
		mmu2d_free_arena(ctxt->mmu, ctxt->allocated);
		ctxt->allocated = ctxt->allocated->next;
	}

	while (ctxt->vacant != NULL) {
		mmu2d_free_arena(ctxt->mmu, ctxt->vacant);
		ctxt->vacant = ctxt->vacant->next;
	}

	ctxt->mmu->refcount -= 1;
	ctxt->mmu = NULL;

	return GCERR_NONE;
}

enum gcerror mmu2d_set_master(struct mmu2dcontext *ctxt)
{
#if MMU_ENABLE
	enum gcerror gcerror;
	struct gcmommumaster *gcmommumaster;
	struct gcmommuinit *gcmommuinit;
	unsigned int size, status, enabled;
	struct mmu2dprivate *mmu = get_mmu();

	if ((ctxt == NULL) || (ctxt->mmu == NULL))
		return GCERR_MMU_CTXT_BAD;

	/* Read the MMU status. */
	status = gc_read_reg(GCREG_MMU_CONTROL_Address);
	enabled = GETFIELD(status, GCREG_MMU_CONTROL, ENABLE);

	/* Is MMU enabled? */
	if (enabled) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"gcx: mmu is already enabled.\n",
			__func__, __LINE__);

		/* Allocate command buffer space. */
		gcerror = cmdbuf_alloc(sizeof(struct gcmommumaster),
					(void **) &gcmommumaster, NULL);
		if (gcerror != GCERR_NONE)
			return GCERR_SETGRP(gcerror, GCERR_MMU_MTLB_SET);

		/* Program master table address. */
		gcmommumaster->master_ldst = gcmommumaster_master_ldst;
		gcmommumaster->master = ctxt->physical;
	} else {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"gcx: mmu is disabled, enabling.\n",
			__func__, __LINE__);

		/* MMU disabled, force physical mode. */
		cmdbuf_physical(true);

		/* Allocate command buffer space. */
		size = sizeof(struct gcmommuinit) + cmdbuf_flush(NULL);
		gcerror = cmdbuf_alloc(size, (void **) &gcmommuinit, NULL);
		if (gcerror != GCERR_NONE)
			return GCERR_SETGRP(gcerror, GCERR_MMU_INIT);

		/* Program the safe zone and the master table address. */
		gcmommuinit->safe_ldst = gcmommuinit_safe_ldst;
		gcmommuinit->safe = mmu->safezone.physical;
		gcmommuinit->mtlb = ctxt->physical;

		/* Execute the buffer. */
		cmdbuf_flush(gcmommuinit + 1);

		/* Resume normal mode. */
		cmdbuf_physical(false);

		/*
		* Enable MMU. For security reasons, once it is enabled,
		* the only way to disable is to reset the system.
		*/
		gc_write_reg(
			GCREG_MMU_CONTROL_Address,
			SETFIELDVAL(0, GCREG_MMU_CONTROL, ENABLE, ENABLE));
	}

	return GCERR_NONE;
#else
	if ((ctxt == NULL) || (ctxt->mmu == NULL))
		return GCERR_MMU_CTXT_BAD;

	return GCERR_NONE;
#endif
}

enum gcerror mmu2d_map(struct mmu2dcontext *ctxt, struct mmu2dphysmem *mem,
			struct mmu2darena **mapped)
{
	enum gcerror gcerror = GCERR_NONE;
	struct mmu2darena *prev, *vacant, *split;
#if MMU_ENABLE
	struct mmu2dstlb *stlb = NULL;
	struct mmu2dstlb **stlb_array;
	u32 *mtlb_logical, *stlb_logical;
#endif
	u32 mtlb_idx, stlb_idx, next_idx;
#if MMU_ENABLE
	u32 i, j, count, available;
#else
	u32 i, count, available;
#endif
	pte_t *parray_alloc = NULL;
	pte_t *parray;

	if ((ctxt == NULL) || (ctxt->mmu == NULL))
		return GCERR_MMU_CTXT_BAD;

	if ((mem == NULL) || (mem->count <= 0) || (mapped == NULL) ||
		((mem->pagesize != 0) && (mem->pagesize != MMU_PAGE_SIZE)))
		return GCERR_MMU_ARG;

	/*
	 * Find available sufficient arena.
	 */

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"mapping (%d) pages\n",
		__func__, __LINE__, mem->count);

	prev = NULL;
	vacant = ctxt->vacant;

	while (vacant != NULL) {
		if (vacant->count >= mem->count)
			break;
		prev = vacant;
		vacant = vacant->next;
	}

	if (vacant == NULL) {
		gcerror = GCERR_MMU_OOM;
		goto fail;
	}

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"found vacant arena:\n",
		__func__, __LINE__);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  mtlb=%d\n",
		__func__, __LINE__, vacant->mtlb);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  stlb=%d\n",
		__func__, __LINE__, vacant->stlb);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  count=%d\n",
		__func__, __LINE__, vacant->count);

	/*
	 * Create page array.
	 */

	/* Reset page array. */
	vacant->pages = NULL;

	/* No page array given? */
	if (mem->pages == NULL) {
		/* Allocate physical address array. */
		parray_alloc = kmalloc(mem->count * sizeof(pte_t *),
					GFP_KERNEL);
		if (parray_alloc == NULL) {
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_MMU_PHYS_ALLOC);
			goto fail;
		}

		/* Fetch page addresses. */
		gcerror = get_physical_pages(mem, parray_alloc, vacant);
		if (gcerror != GCERR_NONE)
			goto fail;

		parray = parray_alloc;

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"physical page array allocated (0x%08X)\n",
			__func__, __LINE__, (unsigned int) parray);
	} else {
		parray = mem->pages;

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"physical page array provided (0x%08X)\n",
			__func__, __LINE__, (unsigned int) parray);
	}

	/*
	 * Allocate slave tables as necessary.
	 */

	mtlb_idx = vacant->mtlb;
	stlb_idx = vacant->stlb;
	count = mem->count;

#if MMU_ENABLE
	mtlb_logical = &ctxt->master.logical[mtlb_idx];
	stlb_array = &ctxt->slave[mtlb_idx];
#endif

	for (i = 0; count > 0; i += 1) {
#if MMU_ENABLE
		if (mtlb_logical[i] == MMU_MTLB_ENTRY_VACANT) {
			gcerror = mmu2d_allocate_slave(ctxt, &stlb);
			if (gcerror != GCERR_NONE)
				goto fail;

			mtlb_logical[i]
				= (stlb->pages.physical & MMU_MTLB_SLAVE_MASK)
				| MMU_MTLB_4K_PAGE
				| MMU_MTLB_EXCEPTION
				| MMU_MTLB_PRESENT;

			stlb_array[i] = stlb;
		}
#endif

		available = MMU_STLB_ENTRY_NUM - stlb_idx;

		if (available > count) {
			available = count;
			next_idx = stlb_idx + count;
		} else {
			mtlb_idx += 1;
			next_idx = 0;
		}

#if MMU_ENABLE
		stlb_logical = &stlb_array[i]->pages.logical[stlb_idx];
		stlb_array[i]->count += available;

		for (j = 0; j < available; j += 1) {
			stlb_logical[j]
				= (*parray & MMU_STLB_ADDRESS_MASK)
				| MMU_STLB_PRESENT
				| MMU_STLB_EXCEPTION
				| MMU_STLB_WRITEABLE;

			parray += 1;
		}

		gc_flush_pages(&stlb_array[i]->pages);
#endif

		count -= available;
		stlb_idx = next_idx;
	}

#if MMU_ENABLE
	gc_flush_pages(&ctxt->master);
#endif

	/*
	 * Claim arena.
	 */

	mem->pagesize = MMU_PAGE_SIZE;

	if (vacant->count != mem->count) {
		gcerror = mmu2d_get_arena(ctxt->mmu, &split);
		if (gcerror != GCERR_NONE)
			goto fail;

		split->mtlb  = mtlb_idx;
		split->stlb  = stlb_idx;
		split->count = vacant->count - mem->count;
		split->next  = vacant->next;
		vacant->next = split;
		vacant->count = mem->count;
	}

	if (prev == NULL)
		ctxt->vacant = vacant->next;
	else
		prev->next = vacant->next;

	vacant->next = ctxt->allocated;
	ctxt->allocated = vacant;

	*mapped = vacant;

#if MMU_ENABLE
	vacant->address
		= ((vacant->mtlb << MMU_MTLB_SHIFT) & MMU_MTLB_MASK)
		| ((vacant->stlb << MMU_STLB_SHIFT) & MMU_STLB_MASK)
		| (mem->offset & MMU_OFFSET_MASK);
#else
	vacant->address = mem->offset + ((parray_alloc == NULL)
		? *mem->pages : *parray_alloc);
#endif

	vacant->size = mem->count * MMU_PAGE_SIZE - mem->offset;

fail:
	if (parray_alloc != NULL) {
		kfree(parray_alloc);

		if (gcerror != GCERR_NONE)
			release_physical_pages(vacant);
	}

	return gcerror;
}

enum gcerror mmu2d_unmap(struct mmu2dcontext *ctxt, struct mmu2darena *mapped)
{
	enum gcerror gcerror = GCERR_NONE;
	struct mmu2darena *prev, *allocated, *vacant;
#if MMU_ENABLE
	struct mmu2dstlb *stlb;
#endif
	u32 mtlb_idx, stlb_idx;
	u32 next_mtlb_idx, next_stlb_idx;
#if MMU_ENABLE
	u32 i, j, count, available;
	u32 *stlb_logical;
#else
	u32 i, count, available;
#endif

	if ((ctxt == NULL) || (ctxt->mmu == NULL))
		return GCERR_MMU_CTXT_BAD;

	/*
	 * Find the arena.
	 */

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"unmapping arena 0x%08X\n",
		__func__, __LINE__, (unsigned int) mapped);

	prev = NULL;
	allocated = ctxt->allocated;

	while (allocated != NULL) {
		if (allocated == mapped)
			break;
		prev = allocated;
		allocated = allocated->next;
	}

	/* The allocation is not listed. */
	if (allocated == NULL) {
		gcerror = GCERR_MMU_ARG;
		goto fail;
	}

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"found allocated arena:\n",
		__func__, __LINE__);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  mtlb=%d\n",
		__func__, __LINE__, allocated->mtlb);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  stlb=%d\n",
		__func__, __LINE__, allocated->stlb);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  count=%d\n",
		__func__, __LINE__, allocated->count);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  address=0x%08X\n",
		__func__, __LINE__, allocated->address);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  logical=0x%08X\n",
		__func__, __LINE__, (unsigned int) allocated->logical);
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  pages=0x%08X\n",
		__func__, __LINE__, (unsigned int) allocated->pages);

	mtlb_idx = allocated->mtlb;
	stlb_idx = allocated->stlb;

	/*
	 * Free slave tables.
	 */

	count = allocated->count;

	for (i = 0; count > 0; i += 1) {
		available = MMU_STLB_ENTRY_NUM - stlb_idx;

		if (available > count) {
			available = count;
			next_mtlb_idx = mtlb_idx;
			next_stlb_idx = stlb_idx + count;
		} else {
			next_mtlb_idx = mtlb_idx + 1;
			next_stlb_idx = 0;
		}

#if MMU_ENABLE
		stlb = ctxt->slave[mtlb_idx];
		if (stlb == NULL) {
			gcerror = GCERR_MMU_ARG;
			goto fail;
		}

		if (stlb->count < available) {
			gcerror = GCERR_MMU_ARG;
			goto fail;
		}

		stlb_logical = &stlb->pages.logical[stlb_idx];
		for (j = 0; j < available; j += 1)
			stlb_logical[j] = MMU_STLB_ENTRY_VACANT;

		stlb->count -= available;
#endif

		count -= available;
		mtlb_idx = next_mtlb_idx;
		stlb_idx = next_stlb_idx;
	}

	/*
	 * Remove from allocated arenas.
	 */

	if (prev == NULL)
		ctxt->allocated = allocated->next;
	else
		prev->next = allocated->next;

	release_physical_pages(allocated);

	/*
	 * Find point of insertion for the arena.
	 */

	prev = NULL;
	vacant = ctxt->vacant;

	while (vacant != NULL) {
		if ((vacant->mtlb > allocated->mtlb) ||
			((vacant->mtlb == allocated->mtlb) &&
			 (vacant->stlb  > allocated->stlb)))
			break;
		prev = vacant;
		vacant = vacant->next;
	}

	/* Insert between the previous and the next vacant arenas. */
	if (mmu2d_siblings(prev, allocated)) {
		if (mmu2d_siblings(allocated, vacant)) {
			prev->count += allocated->count;
			prev->count += vacant->count;
			prev->next   = vacant->next;
			mmu2d_free_arena(ctxt->mmu, allocated);
			mmu2d_free_arena(ctxt->mmu, vacant);
		} else {
			prev->count += allocated->count;
			mmu2d_free_arena(ctxt->mmu, allocated);
		}
	} else if (mmu2d_siblings(allocated, vacant)) {
		vacant->mtlb   = allocated->mtlb;
		vacant->stlb   = allocated->stlb;
		vacant->count += allocated->count;
		mmu2d_free_arena(ctxt->mmu, allocated);
	} else {
		allocated->next = vacant;
		if (prev == NULL)
			ctxt->vacant = allocated;
		else
			prev->next = allocated;
	}

fail:
	return gcerror;
}

int mmu2d_flush(void *logical, u32 address, u32 size)
{
#if MMU_ENABLE
	static const int flushSize = sizeof(struct gcmommuflush);
	struct gcmommuflush *gcmommuflush;
	u32 count;

	GCPRINT(GCDBGFILTER, GCZONE_FLUSH, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	if (logical != NULL) {
		GCPRINT(GCDBGFILTER, GCZONE_FLUSH, GC_MOD_PREFIX
			"address = 0x%08X\n",
			__func__, __LINE__, address);

		GCPRINT(GCDBGFILTER, GCZONE_FLUSH, GC_MOD_PREFIX
			"size = %d\n",
			__func__, __LINE__, size);

		/* Compute the buffer count. */
		count = (size - flushSize + 7) >> 3;

		gcmommuflush = (struct gcmommuflush *) logical;

		/* Flush 2D PE cache. */
		gcmommuflush->peflush.flush_ldst = gcmoflush_flush_ldst;
		gcmommuflush->peflush.flush.reg = gcregflush_pe2D;

		/* Arm the FE-PE semaphore. */
		gcmommuflush->peflushsema.sema_ldst = gcmosema_sema_ldst;
		gcmommuflush->peflushsema.sema.reg  = gcregsema_fe_pe;

		/* Stall FE until PE is done flushing. */
		gcmommuflush->peflushstall.cmd.fld = gcfldstall;
		gcmommuflush->peflushstall.arg.fld = gcfldstall_fe_pe;

		/* LINK to the next slot to flush FE FIFO. */
		gcmommuflush->feflush.cmd.fld = gcfldlink4;
		gcmommuflush->feflush.address
			= address
			+ offsetof(struct gcmommuflush, mmuflush_ldst);

		/* Flush MMU cache. */
		gcmommuflush->mmuflush_ldst = gcmommuflush_mmuflush_ldst;
		gcmommuflush->mmuflush.reg = gcregmmu_flush;

		/* Arm the FE-PE semaphore. */
		gcmommuflush->mmuflushsema.sema_ldst = gcmosema_sema_ldst;
		gcmommuflush->mmuflushsema.sema.reg  = gcregsema_fe_pe;

		/* Stall FE until PE is done flushing. */
		gcmommuflush->mmuflushstall.cmd.fld = gcfldstall;
		gcmommuflush->mmuflushstall.arg.fld = gcfldstall_fe_pe;

		/* LINK to the next slot to flush FE FIFO. */
		gcmommuflush->link.cmd.fld.opcode
			= GCREG_COMMAND_LINK_COMMAND_OPCODE_LINK;
		gcmommuflush->link.cmd.fld.count = count;
		gcmommuflush->link.address = address + flushSize;
	}

	GCPRINT(GCDBGFILTER, GCZONE_FLUSH, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* Return the size in bytes required for the flush. */
	return flushSize;
#else
	return 0;
#endif
}

enum gcerror mmu2d_fixup(struct gcfixup *fixup, unsigned int *data)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcfixupentry *table;
	struct mmu2darena *arena;
	unsigned int dataoffset;
	unsigned int surfoffset;
	unsigned int i;

	/* Process fixups. */
	while (fixup != NULL) {
		GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
			"processing %d fixup(s) @ 0x%08X\n",
			__func__, __LINE__, fixup->count, (unsigned int) fixup);

		/* Apply fixups. */
		table = fixup->fixup;
		for (i = 0; i < fixup->count; i += 1) {
			GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
				"[%02d] buffer offset = 0x%08X, "
				"surface offset = 0x%08X\n",
				__func__, __LINE__, i,
				table->dataoffset * 4,
				table->surfoffset);

			dataoffset = table->dataoffset;
			arena = (struct mmu2darena *) data[dataoffset];

			GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
				"arena = 0x%08X\n",
				__func__, __LINE__,  (unsigned int) arena);
			GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
				"arena phys = 0x%08X\n",
				__func__, __LINE__, arena->address);
			GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
				"arena size = %d\n",
				__func__, __LINE__, arena->size);

			surfoffset = table->surfoffset;

#if 0
			if (surfoffset > arena->size) {
				gcerror = GCERR_MMU_OFFSET;
				goto exit;
			}
#endif

			data[dataoffset] = arena->address + surfoffset;

			GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
				"surface address = 0x%08X\n",
				__func__, __LINE__, data[dataoffset]);

#if GC_FLUSH_USER_PAGES
			flush_user_buffer(arena);
#endif

			table += 1;
		}

		/* Get the next fixup. */
		fixup = fixup->next;
	}

	return gcerror;
}
