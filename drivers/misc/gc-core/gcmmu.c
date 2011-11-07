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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/io.h>

#include "gcreg.h"
#include "gcmmu.h"
#include "gccmdbuf.h"

/*
 * Debugging.
 */

typedef u32 (*pfn_get_present) (u32 entry);
typedef void (*pfn_print_entry) (u32 index, u32 entry);

struct mm2dtable {
	char *name;
	u32 entry_count;
	u32 vacant_entry;
	pfn_get_present get_present;
	pfn_print_entry print_entry;
};

u32 get_mtlb_present(u32 entry)
{
	return entry & MMU_MTLB_PRESENT_MASK;
}

u32 get_stlb_present(u32 entry)
{
	return entry & MMU_STLB_PRESENT_MASK;
}

void print_mtlb_entry(u32 index, u32 entry)
{
	MMU2D_PRINT(KERN_ERR
		"  entry[%03d]: 0x%08X (stlb=0x%08X, ps=%d, ex=%d, pr=%d)\n",
			index,
			entry,
			entry & MMU_MTLB_SLAVE_MASK,
			(entry & MMU_MTLB_PAGE_SIZE_MASK) >> 2,
			(entry & MMU_MTLB_EXCEPTION_MASK) >> 1,
			(entry & MMU_MTLB_PRESENT_MASK)
			);
}

void print_stlb_entry(u32 index, u32 entry)
{
	MMU2D_PRINT(KERN_ERR
		"  entry[%03d]: 0x%08X (user=0x%08X, wr=%d, ex=%d, pr=%d)\n",
			index,
			entry,
			entry & MMU_STLB_ADDRESS_MASK,
			(entry & MMU_STLB_WRITEABLE_MASK) >> 2,
			(entry & MMU_STLB_EXCEPTION_MASK) >> 1,
			(entry & MMU_STLB_PRESENT_MASK)
			);
}

static void mmu2d_dump_table(struct mm2dtable *desc, struct mmu2dpage *table)
{
	int present, vacant, skipped;
	u32 *logical;
	u32 entry;
	u32 i;

	if (table->size == 0) {
		MMU2D_PRINT(KERN_ERR "%s table is not allocated.\n",
								desc->name);
		return;
	}

	MMU2D_PRINT(KERN_ERR "\n%s table:\n", desc->name);
	MMU2D_PRINT(KERN_ERR "  physical=0x%08X\n", (u32) table->physical);
	MMU2D_PRINT(KERN_ERR "  size=%d\n", table->size);

	vacant = -1;
	logical = table->logical;

	for (i = 0; i < desc->entry_count; i += 1) {
		entry = logical[i];

		present = desc->get_present(entry);

		if (!present && (entry == desc->vacant_entry)) {
			if (vacant == -1)
				vacant = i;
			continue;
		}

		if (vacant != -1) {
			skipped = i - vacant;
			vacant = -1;
			MMU2D_PRINT(KERN_ERR
			"              skipped %d vacant entries\n", skipped);
		}

		if (present) {
			desc->print_entry(i, entry);
		} else {
			MMU2D_PRINT(KERN_ERR
		"  entry[%03d]: invalid entry value (0x%08X)\n", i, entry);
		}
	}

	if (vacant != -1) {
		skipped = i - vacant;
		vacant = -1;
		MMU2D_PRINT(KERN_ERR
		"              skipped %d vacant entries\n", skipped);
	}
}

/*
 * Page allocation logic. This should be a part of the kernel driver memory
 * manager, for the lack of which I need to have these here.
 */

int mmu2d_alloc_pages(struct mmu2dpage *p, u32 size)
{
	int ret;
	int order;

	p->pages = NULL;
	p->logical = NULL;
	p->physical = ~0UL;

	order = get_order(size);
	MMU2D_PRINT("%s(%d): size=%d, order=%d\n", __func__, __LINE__,
								size, order);

	p->order = order;
	p->size = (1 << order) * MMU_PAGE_SIZE;

	p->pages = alloc_pages(GFP_KERNEL, order);
	if (p->pages == NULL) {
		MMU2D_PRINT("%s(%d): alloc_pages failed!\n", __func__,
								__LINE__);
		ret = -ENOMEM;
		goto fail;
	}

	p->physical = page_to_phys(p->pages);

	p->logical = (u32 *) ioremap_nocache(p->physical, size);
	if (p->logical == NULL) {
		MMU2D_PRINT("%s(%d): kmap failed!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto fail;
	}

	MMU2D_PRINT("%s(%d): physical=0x%08X, size=%d\n", __func__, __LINE__,
						(u32) p->physical, p->size);
	return 0;

fail:
	mmu2d_free_pages(p);
	return ret;
}

void mmu2d_free_pages(struct mmu2dpage *p)
{
	if (p->logical != NULL) {
		iounmap(p->logical);
		p->logical = NULL;
	}

	if (p->pages != NULL) {
		__free_pages(p->pages, p->order);
		p->pages = NULL;
	}

	p->physical = ~0UL;
	p->order = 0;
	p->size = 0;
}

/*
 * Arena record management.
 */

static int mmu2d_get_arena(struct mmu2dprivate *mmu, struct mmu2darena **arena)
{
	int i;
	struct mmu2darenablock *block;
	struct mmu2darena *temp;

	if (mmu->arena_recs == NULL) {
		MMU2D_PRINT(KERN_ERR "%s(%d): allocating arenas\n", __func__,
								__LINE__);
		block = kmalloc(ARENA_PREALLOC_SIZE, GFP_KERNEL);
		if (block == NULL)
			return -ENOMEM;

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

	MMU2D_PRINT(KERN_ERR "%s(%d): allocated 0x%p\n", __func__, __LINE__,
									*arena);
	return 0;
}

static void mmu2d_free_arena(struct mmu2dprivate *mmu, struct mmu2darena *arena)
{
	/* Add back to the available arena list. */
	arena->next = mmu->arena_recs;
	mmu->arena_recs = arena;
}

static int mmu2d_siblings(struct mmu2darena *arena1, struct mmu2darena *arena2)
{
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
	}

	return ((mtlb_idx == arena2->mtlb) && (stlb_idx == arena2->stlb));
}

/*
 * Slave table allocation management.
 */

static int mmu2d_allocate_slave(struct mmu2dcontext *ctxt,
				struct mmu2dstlb **stlb)
{
	int i, ret;
	struct mmu2dstlbblock *block;
	struct mmu2dstlb *temp;

	if (ctxt->slave_recs == NULL) {
		block = kmalloc(STLB_PREALLOC_SIZE, GFP_KERNEL);
		if (block == NULL)
			return -ENOMEM;

		block->next = ctxt->slave_blocks;
		ctxt->slave_blocks = block;

		temp = (struct mmu2dstlb *)(block + 1);
		for (i = 0; i < STLB_PREALLOC_COUNT; i += 1) {
			temp->next = ctxt->slave_recs;
			ctxt->slave_recs = temp;
			temp += 1;
		}
	}

	ret = mmu2d_alloc_pages(&ctxt->slave_recs->pages, MMU_STLB_SIZE);
	if (ret != 0)
		return ret;

	/* Remove from the list of available records. */
	temp = ctxt->slave_recs;
	ctxt->slave_recs = ctxt->slave_recs->next;

	/* Invalidate all entries. */
	for (i = 0; i < MMU_STLB_ENTRY_NUM; i += 1)
		temp->pages.logical[i] = MMU_STLB_ENTRY_VACANT;

	/* Reset allocated entry count. */
	temp->count = 0;

	*stlb = temp;
	return 0;
}

static void mmu2d_free_slave(struct mmu2dcontext *ctxt, struct mmu2dstlb *slave)
{
	mmu2d_free_pages(&slave->pages);
	slave->next = ctxt->slave_recs;
	ctxt->slave_recs = slave;
}

int mmu2d_create_context(struct mmu2dcontext *ctxt)
{
	int i, ret;
	struct mmu2dprivate *mmu = NULL;
	u32 *buffer;
	u32 physical;

	if (ctxt == NULL)
		return -EINVAL;

	mmu = get_mmu();
	if (mmu == NULL)
		return -ENOMEM;

	memset(ctxt, 0, sizeof(struct mmu2dcontext));
	sema_init(&ctxt->pts, 1);

	/* Allocate MTLB table. */
	ret = mmu2d_alloc_pages(&ctxt->master, MMU_MTLB_SIZE);
	if (ret != 0)
		goto fail;

	/* Allocate an array of pointers to slave descriptors. */
	ctxt->slave = kmalloc(MMU_MTLB_SIZE, GFP_KERNEL);
	if (ctxt->slave == NULL) {
		ret = -ENOMEM;
		goto fail;
	}
	memset(ctxt->slave, 0, MMU_MTLB_SIZE);

	/* Invalidate all entries. */
	for (i = 0; i < MMU_MTLB_ENTRY_NUM; i += 1)
		ctxt->master.logical[i] = MMU_MTLB_ENTRY_VACANT;

	/* Configure the physical address. */
	ctxt->physical = SETFIELD(~0U, GCREG_MMU_CONFIGURATION, ADDRESS,
		(ctxt->master.physical >>
					GCREG_MMU_CONFIGURATION_ADDRESS_Start))
		& SETFIELDVAL(~0U, GCREG_MMU_CONFIGURATION, MASK_ADDRESS,
								ENABLED)
		& SETFIELD(~0U, GCREG_MMU_CONFIGURATION, MODE, MMU_MTLB_MODE)
		& SETFIELDVAL(~0U, GCREG_MMU_CONFIGURATION, MASK_MODE, ENABLED);

	ret = mmu2d_get_arena(mmu, &ctxt->vacant);
	if (ret != 0)
		goto fail;

	ctxt->vacant->mtlb  = 0;
	ctxt->vacant->stlb  = 0;
	ctxt->vacant->count = MMU_MTLB_ENTRY_NUM * MMU_STLB_ENTRY_NUM;
	ctxt->vacant->next  = NULL;

	ctxt->allocated = NULL;

	if (mmu->safezone.size == 0) {
		/* MMU should not be enabled at this point yet. */
		if (mmu->enabled) {
			ret = -EINVAL;
			goto fail;
		}

		/* Allocate the safe zone. */
		ret = mmu2d_alloc_pages(&mmu->safezone, MMU_SAFE_ZONE_SIZE);
		if (ret != 0)
			goto fail;

		/* Initialize safe zone to a value. */
		for (i = 0; i < MMU_SAFE_ZONE_SIZE / sizeof(u32); i += 1)
			mmu->safezone.logical[i] = 0xBABEFACE;

		/* First context created, initialize essential MMU pointers. */
		ret = cmdbuf_alloc(4 * sizeof(u32), &buffer, &physical);
		if (ret != 0)
			goto fail;

		/* Once the safe address is programmed, it cannot be changed. */
		buffer[0]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
			      LOAD_STATE)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
			     gcregMMUSafeAddressRegAddrs)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT,
			     1);

		buffer[1]
		= mmu->safezone.physical;

		/* Progfram master table address. */
		buffer[2]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
			      LOAD_STATE)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
			     gcregMMUConfigurationRegAddrs)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT,
			     1);

		buffer[3]
		= ctxt->physical;

		/* Execute the current command buffer. */
		ret = cmdbuf_flush();
		if (ret != 0)
			goto fail;

		/*
		 * Enable MMU. For security reasons, once it is enabled, the
		 * only way to disable is to reset the system.
		 */
		hw_write_reg(
			GCREG_MMU_CONTROL_Address,
			SETFIELDVAL(0, GCREG_MMU_CONTROL, ENABLE, ENABLE));

	}

	/* Reference MMU. */
	mmu->refcount += 1;
	ctxt->mmu = mmu;

	MMU2D_PRINT(KERN_ERR "%s(%d): created 0x%p\n", __func__, __LINE__,
								ctxt);
	return 0;

fail:
	mmu2d_free_pages(&ctxt->master);
	if (ctxt->slave != NULL)
		kfree(ctxt->slave);
	return ret;
}

int mmu2d_destroy_context(struct mmu2dcontext *ctxt)
{
	int i;

	if (ctxt == NULL)
		return -EINVAL;

	if (ctxt->mmu == NULL)
		return 0;

	if (ctxt->slave != NULL) {
		for (i = 0; i < MMU_MTLB_ENTRY_NUM; i += 1) {
			if (ctxt->slave[i] != NULL) {
				mmu2d_free_pages(&ctxt->slave[i]->pages);
				ctxt->slave[i] = NULL;
			}
		}
		kfree(ctxt->slave);
		ctxt->slave = NULL;
	}

	while (ctxt->allocated != NULL) {
		mmu2d_free_arena(ctxt->mmu, ctxt->allocated);
		ctxt->allocated = ctxt->allocated->next;
	}

	while (ctxt->vacant != NULL) {
		mmu2d_free_arena(ctxt->mmu, ctxt->vacant);
		ctxt->vacant = ctxt->vacant->next;
	}

	mmu2d_free_pages(&ctxt->master);

	ctxt->mmu->refcount -= 1;
	ctxt->mmu = NULL;

	return 0;
}

int mmu2d_set_master(struct mmu2dcontext *ctxt)
{
	int ret;
	u32 *buffer;
	u32 physical;

	if (ctxt == NULL)
		return -EINVAL;

	if (ctxt->mmu == NULL)
		return 0;

    /* First context created, initialize essential MMU pointers. */
	ret = cmdbuf_alloc(2 * sizeof(u32), &buffer, &physical);
	if (ret != 0) {
		MMU2D_PRINT(KERN_ERR "%s(%d)\n", __func__, __LINE__);
		goto fail;
	}

	/* Progfram master table address. */
	buffer[0]
	= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
			    LOAD_STATE)
		| SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
			    gcregMMUConfigurationRegAddrs)
		| SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT,
			    1);

	buffer[1]
	= ctxt->physical;

fail:
	return 0;
}

int mmu2d_map_phys(struct mmu2dcontext *ctxt,
		   struct mmu2dphysmem *mem)
{
	int ret = 0;
	struct mmu2darena *prev, *vacant;
	struct mmu2dstlb *stlb;
	struct mmu2darena *arena;
	u32 *mtlb_logical, *stlb_logical;
	u32 mtlb_idx, stlb_idx, next_idx;
	u32 i, j, count, available;
	u32 *user_pages;

	if (ctxt == NULL)
		return -EINVAL;

	if (ctxt->mmu == NULL)
		return -EINVAL;

	if ((mem == NULL) || (mem->pagecount <= 0) || (mem->pages == NULL) ||
			((mem->pagesize != 0) &&
				(mem->pagesize != MMU_PAGE_SIZE))) {
		return -EINVAL;
	}

	down(&ctxt->pts);

#if MMU2D_DUMP
	MMU2D_PRINT(KERN_ERR "%s(%d): mapping (%d) pages:\n",
		__func__, __LINE__, mem->pagecount);

	for (i = 0; i < mem->pagecount; i += 1)
		MMU2D_PRINT(KERN_ERR "  %d: 0x%08X\n", i, (u32) mem->pages[i]);
#endif

	/*
	 * Find available sufficient arena.
	 */

	prev = NULL;
	vacant = ctxt->vacant;

	while (vacant != NULL) {
		if (vacant->count >= mem->pagecount)
			break;
		prev = vacant;
		vacant = vacant->next;
	}

	if (vacant == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	/*
	 * Allocate slave tables as necessary.
	 */

	mtlb_idx = vacant->mtlb;
	stlb_idx = vacant->stlb;
	count = mem->pagecount;

	mtlb_logical = &ctxt->master.logical[mtlb_idx];
	user_pages = (u32 *) mem->pages;

	for (i = 0; count > 0; i += 1) {
		if (mtlb_logical[i] == MMU_MTLB_ENTRY_VACANT) {
			ret = mmu2d_allocate_slave(ctxt, &stlb);
			if (ret != 0)
				goto fail;

			mtlb_logical[i]
			= (stlb->pages.physical & MMU_MTLB_SLAVE_MASK)
			  | MMU_MTLB_4K_PAGE
			  | MMU_MTLB_EXCEPTION
			  | MMU_MTLB_PRESENT;

			ctxt->slave[i] = stlb;
		}

		available = MMU_STLB_ENTRY_NUM - stlb_idx;

		if (available > count) {
			available = count;
			next_idx = stlb_idx + count;
		} else {
			mtlb_idx += 1;
			next_idx = 0;
		}

		stlb_logical = &ctxt->slave[i]->pages.logical[stlb_idx];
		ctxt->slave[i]->count += available;

		for (j = 0; j < available; j += 1) {
			stlb_logical[j]
			= (*user_pages & MMU_STLB_ADDRESS_MASK)
			  | MMU_STLB_PRESENT
			  | MMU_STLB_EXCEPTION
			  | MMU_STLB_WRITEABLE;

			user_pages += 1;
		}

		count -= available;
		stlb_idx = next_idx;
	}

	/*
	 * Claim arena.
	 */

	mem->logical
	= ((vacant->mtlb << MMU_MTLB_SHIFT) & MMU_MTLB_MASK)
	  | ((vacant->stlb << MMU_STLB_SHIFT) & MMU_STLB_MASK)
	  | (mem->pageoffset & MMU_OFFSET_MASK);

	mem->pagesize = MMU_PAGE_SIZE;

	MMU2D_PRINT(KERN_ERR "%s(%d): mapped to 0x%08X\n",
		__func__, __LINE__, mem->logical);

	if (vacant->count == mem->pagecount) {
		if (prev == NULL)
			ctxt->vacant = vacant->next;
		else
			prev->next = vacant->next;
		vacant->next = ctxt->allocated;
		ctxt->allocated = vacant;
	} else {
		ret = mmu2d_get_arena(ctxt->mmu, &arena);
		if (ret != 0)
			goto fail;

		arena->mtlb  = vacant->mtlb;
		arena->stlb  = vacant->stlb;
		arena->count = mem->pagecount;
		arena->next  = ctxt->allocated;
		ctxt->allocated = arena;

		vacant->mtlb   = mtlb_idx;
		vacant->stlb   = stlb_idx;
		vacant->count -= mem->pagecount;
	}

fail:
	up(&ctxt->pts);
	return ret;
}

int mmu2d_unmap(struct mmu2dcontext *ctxt,
		struct mmu2dphysmem *mem)
{
	int ret;
	struct mmu2darena *prev, *allocated, *vacant;
	struct mmu2dstlb *stlb;
	u32 mtlb_idx, stlb_idx;
	u32 next_mtlb_idx, next_stlb_idx;
	u32 i, j, count, available;
	u32 *stlb_logical;

	if (ctxt == NULL)
		return -EINVAL;

	if (ctxt->mmu == NULL)
		return -EINVAL;

	down(&ctxt->pts);

	/*
	 * Find the arena.
	 */

	mtlb_idx = (mem->logical & MMU_MTLB_MASK) >> MMU_MTLB_SHIFT;
	stlb_idx = (mem->logical & MMU_STLB_MASK) >> MMU_STLB_SHIFT;

	prev = NULL;
	allocated = ctxt->allocated;

	while (allocated != NULL) {
		if ((allocated->mtlb == mtlb_idx) &&
				(allocated->stlb == stlb_idx))
			break;
		prev = allocated;
		allocated = allocated->next;
	}

	if (allocated == NULL) {
		ret = -EINVAL;
		goto fail;
	}

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

		stlb = ctxt->slave[mtlb_idx];
		if (stlb == NULL) {
			ret = -EINVAL;
			goto fail;
		}

		if (stlb->count < available) {
			ret = -EINVAL;
			goto fail;
		}

		stlb_logical = &stlb->pages.logical[stlb_idx];
		for (j = 0; j < available; j += 1)
			stlb_logical[j] = MMU_STLB_ENTRY_VACANT;

		stlb->count -= available;
		if (stlb->count == 0) {
			mmu2d_free_slave(ctxt, stlb);
			ctxt->slave[mtlb_idx] = NULL;
			ctxt->master.logical[mtlb_idx] = MMU_MTLB_ENTRY_VACANT;
		}

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

	/*
	 * Find point of insertion for the arena.
	 */

	prev = NULL;
	vacant = ctxt->vacant;

	while (vacant != NULL) {
		if ((vacant->mtlb >= allocated->mtlb) &&
				(vacant->stlb > allocated->stlb))
			break;
		prev = vacant;
		vacant = vacant->next;
	}

	if (prev == NULL) {
		if (vacant == NULL) {
			allocated->next = ctxt->vacant;
			ctxt->vacant = allocated;
		} else {
			if (mmu2d_siblings(allocated, vacant)) {
				vacant->mtlb   = allocated->mtlb;
				vacant->stlb   = allocated->stlb;
				vacant->count += allocated->count;
				mmu2d_free_arena(ctxt->mmu, allocated);
			} else {
				allocated->next = ctxt->vacant;
				ctxt->vacant = allocated;
			}
		}
	} else {
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
			prev->next = allocated;
		}
	}

fail:
	up(&ctxt->pts);
	return ret;
}

int mmu2d_flush(u32 *logical, u32 address, u32 size)
{
	static const int flushSize = 16 * sizeof(u32);
	u32 count;

	if (size != 0) {
		/* Compute the buffer count. */
		count = (size + 7) >> 3;

		/* Flush 2D PE cache. */
		logical[0]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
								LOAD_STATE)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 0x1)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
							AQFlushRegAddrs);
		logical[1]
		= SETFIELDVAL(0, AQ_FLUSH, PE2D_CACHE, ENABLE);

		/* Arm the FE-PE semaphore. */
		logical[2]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
								LOAD_STATE)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
							AQSemaphoreRegAddrs)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

		logical[3]
		= SETFIELDVAL(0, AQ_SEMAPHORE, SOURCE, FRONT_END)
		  | SETFIELDVAL(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE);

		/* Stall FE until PE is done flushing. */
		logical[4]
		= SETFIELDVAL(0, STALL_COMMAND, OPCODE, STALL);

		logical[5]
		= SETFIELDVAL(0, AQ_SEMAPHORE, SOURCE, FRONT_END)
		  | SETFIELDVAL(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE);

		/* LINK to the next slot to flush FE FIFO. */
		logical[6]
		= SETFIELDVAL(0, AQ_COMMAND_LINK_COMMAND, OPCODE, LINK)
		  | SETFIELD(0, AQ_COMMAND_LINK_COMMAND, PREFETCH, 4);

		logical[7] = address + 8 * sizeof(u32);

		/* Flush MMU cache. */
		logical[8]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
								LOAD_STATE)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
			     gcregMMUConfigurationRegAddrs)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

		logical[9]
		= SETFIELDVAL(~0U, GCREG_MMU_CONFIGURATION, FLUSH, FLUSH)
		  & SETFIELDVAL(~0U, GCREG_MMU_CONFIGURATION, MASK_FLUSH,
								ENABLED);

		/* Arm the FE-PE semaphore. */
		logical[10]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
								LOAD_STATE)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
							 AQSemaphoreRegAddrs)
		  | SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

		logical[11]
		= SETFIELDVAL(0, AQ_SEMAPHORE, SOURCE, FRONT_END)
		  | SETFIELDVAL(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE);

		/* Stall FE until PE is done flushing. */
		logical[12]
		= SETFIELDVAL(0, STALL_COMMAND, OPCODE, STALL);

		logical[13]
		= SETFIELDVAL(0, AQ_SEMAPHORE, SOURCE, FRONT_END)
		  | SETFIELDVAL(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE);

		/* LINK to the next slot to flush FE FIFO. */
		logical[14]
		= SETFIELDVAL(0, AQ_COMMAND_LINK_COMMAND, OPCODE, LINK)
		  | SETFIELD(0, AQ_COMMAND_LINK_COMMAND, PREFETCH, count);

		logical[15]
		= address + flushSize;
	}

	/* Return the size in bytes required for the flush. */
	return flushSize;
}

void mmu2d_dump(struct mmu2dcontext *ctxt)
{
	static struct mm2dtable mtlb_desc = {
		"Master",
		MMU_MTLB_ENTRY_NUM,
		MMU_MTLB_ENTRY_VACANT,
		get_mtlb_present,
		print_mtlb_entry
	};

	static struct mm2dtable stlb_desc = {
		"Slave",
		MMU_STLB_ENTRY_NUM,
		MMU_STLB_ENTRY_VACANT,
		get_stlb_present,
		print_stlb_entry
	};

	struct mmu2darena *vacant;
	u32 size;
	char *unit;
	int i;

	MMU2D_PRINT(KERN_ERR "\n*** MMU DUMP ***\n");

	if (ctxt->vacant == NULL) {
		MMU2D_PRINT(KERN_ERR "\nNo vacant arenas defined!\n");
	} else {
		vacant = ctxt->vacant;

		while (vacant != NULL) {

			size = vacant->count * 4;

			if (size < 1024) {
				unit = "KB";
			} else {
				size /= 1024;
				if (size < 1024) {
					unit = "MB";
				} else {
					size /= 1024;
					unit = "GB";
				}
			}

			MMU2D_PRINT(KERN_ERR "Vacant arena: 0x%08X\n",
							 (u32) vacant);
			MMU2D_PRINT(KERN_ERR "  mtlb       = %d\n",
							vacant->mtlb);
			MMU2D_PRINT(KERN_ERR "  stlb       = %d\n",
							vacant->stlb);
			MMU2D_PRINT(KERN_ERR "  page count = %d\n",
							vacant->count);
			MMU2D_PRINT(KERN_ERR "  size       = %d%s\n",
							size, unit);

			vacant = vacant->next;
		}
	}

	mmu2d_dump_table(&mtlb_desc, &ctxt->master);

	for (i = 0; i < MMU_MTLB_ENTRY_NUM; i += 1)
		if (ctxt->slave[i] != NULL)
			mmu2d_dump_table(&stlb_desc, &ctxt->slave[i]->pages);
}
