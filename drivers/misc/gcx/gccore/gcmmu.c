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
#include <linux/pagemap.h>
#include <linux/sched.h>
#include "gcmain.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_MAPPING		(1 << 1)
#define GCZONE_CONTEXT		(1 << 2)
#define GCZONE_MASTER		(1 << 3)
#define GCZONE_FIXUP		(1 << 4)
#define GCZONE_FLUSH		(1 << 5)
#define GCZONE_ARENA		(1 << 6)

GCDBG_FILTERDEF(mmu, GCZONE_NONE,
		"init",
		"mapping",
		"context",
		"master",
		"fixup",
		"flush",
		"arena")


/*******************************************************************************
 * Internal definitions.
 */

/* Slave table preallocation block; can describe an array of slave tables. */
struct gcmmustlbblock {
	/* Slave table allocation. */
	struct gcpage pages;

	/* Next block of preallocated slave memory. */
	struct gcmmustlbblock *next;
};


/*******************************************************************************
 * Call back to enable MMU.
 */

static void event_enable_mmu(struct gcevent *gcevent, unsigned int *flags)
{
	GCENTER(GCZONE_INIT);

	/*
	* Enable MMU. For security reasons, once it is enabled,
	* the only way to disable is to reset the system.
	*/
	gc_write_reg(
		GCREG_MMU_CONTROL_Address,
		GCSETFIELDVAL(0, GCREG_MMU_CONTROL, ENABLE, ENABLE));

	/* After MMU command buffer is processed, FE will stop.
	 * Let the control thread know that FE needs to be restarted. */
	if (flags == NULL)
		GCERR("flags are not set.\n");
	else
		*flags |= GC_CMDBUF_START_FE;

	GCEXIT(GCZONE_INIT);
}


/*******************************************************************************
 * Arena record management.
 */

static enum gcerror get_arena(struct gcmmu *gcmmu, struct gcmmuarena **arena)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcmmuarena *temp;

	GCENTER(GCZONE_ARENA);

	GCLOCK(&gcmmu->lock);

	if (list_empty(&gcmmu->vacarena)) {
		temp = kmalloc(sizeof(struct gcmmuarena), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("arena entry allocation failed.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_MMU_ARENA_ALLOC);
			goto exit;
		}
	} else {
		struct list_head *head;
		head = gcmmu->vacarena.next;
		temp = list_entry(head, struct gcmmuarena, link);
		list_del(head);
	}

	*arena = temp;

exit:
	GCUNLOCK(&gcmmu->lock);

	GCEXITARG(GCZONE_ARENA, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

static inline bool siblings(struct list_head *head,
			    struct list_head *arenahead1,
			    struct list_head *arenahead2)
{
	struct gcmmuarena *arena1;
	struct gcmmuarena *arena2;

	if ((arenahead1 == head) || (arenahead2 == head))
		return false;

	arena1 = list_entry(arenahead1, struct gcmmuarena, link);
	arena2 = list_entry(arenahead2, struct gcmmuarena, link);

	return (arena1->end.absolute == arena2->start.absolute) ? true : false;
}

/*******************************************************************************
 * Slave table allocation management.
 */

static enum gcerror allocate_slave(struct gcmmucontext *gcmmucontext,
				   union gcmmuloc index)
{
	enum gcerror gcerror;
	struct gcmmustlbblock *block = NULL;
	struct gcmmustlb *slave;
	unsigned int *mtlblogical;
	unsigned int prealloccount;
	unsigned int preallocsize;
	unsigned int preallocentries;
	unsigned int physical;
	unsigned int *logical;
	unsigned int i;

	GCENTER(GCZONE_MAPPING);

	/* Allocate a new prealloc block wrapper. */
	block = kmalloc(sizeof(struct gcmmustlbblock), GFP_KERNEL);
	if (block == NULL) {
		GCERR("failed to allocate slave page table wrapper\n");
		gcerror = GCERR_SETGRP(GCERR_OODM,
				       GCERR_MMU_STLB_ALLOC);
		goto exit;
	}

	/* Determine the number and the size of tables to allocate. */
	prealloccount = min(GCMMU_STLB_PREALLOC_COUNT,
			    GCMMU_MTLB_ENTRY_NUM - index.loc.mtlb);

	preallocsize = prealloccount * GCMMU_STLB_SIZE;
	preallocentries = prealloccount * GCMMU_STLB_ENTRY_NUM;

	GCDBG(GCZONE_MAPPING, "preallocating %d slave tables.\n",
	      prealloccount);

	/* Allocate slave table pool. */
	gcerror = gc_alloc_cached(&block->pages, preallocsize);
	if (gcerror != GCERR_NONE) {
		GCERR("failed to allocate slave page table\n");
		gcerror = GCERR_SETGRP(gcerror, GCERR_MMU_STLB_ALLOC);
		goto exit;
	}

	/* Add the block to the list. */
	block->next = gcmmucontext->slavealloc;
	gcmmucontext->slavealloc = block;

	/* Get shortcuts to the pointers. */
	physical = block->pages.physical;
	logical = block->pages.logical;

	/* Invalidate all slave entries. */
	for (i = 0; i < preallocentries; i += 1)
		logical[i] = GCMMU_STLB_ENTRY_VACANT;

	/* Init the slaves. */
	slave = &gcmmucontext->slave[index.loc.mtlb];
	mtlblogical = &gcmmucontext->master.logical[index.loc.mtlb];

	for (i = 0; i < prealloccount; i += 1) {
		mtlblogical[i]
			= (physical & GCMMU_MTLB_SLAVE_MASK)
			| GCMMU_MTLB_4K_PAGE
			| GCMMU_MTLB_EXCEPTION
			| GCMMU_MTLB_PRESENT;

		slave[i].physical = physical;
		slave[i].logical = logical;

		physical += GCMMU_STLB_SIZE;
		logical = (unsigned int *)
			((unsigned char *) logical + GCMMU_STLB_SIZE);
	}

	/* Flush CPU cache. */
	gc_flush_region(gcmmucontext->master.physical,
			gcmmucontext->master.logical,
			index.loc.mtlb * sizeof(unsigned int),
			prealloccount * sizeof(unsigned int));

	GCEXIT(GCZONE_MAPPING);
	return GCERR_NONE;

exit:
	if (block != NULL)
		kfree(block);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

/*******************************************************************************
 * Physical page array generation.
 */

static enum gcerror virt2phys(unsigned int logical, pte_t *physical)
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

static enum gcerror get_physical_pages(struct gcmmuphysmem *mem,
					pte_t *parray,
					struct gcmmuarena *arena)
{
	enum gcerror gcerror = GCERR_NONE;
	struct vm_area_struct *vma;
	struct page **pages = NULL;
	unsigned int base, write;
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

static void release_physical_pages(struct gcmmuarena *arena)
{
	unsigned int i;

	if (arena->pages != NULL) {
		for (i = 0; i < arena->count; i += 1)
			page_cache_release(arena->pages[i]);

		kfree(arena->pages);
		arena->pages = NULL;
	}
}

/*******************************************************************************
 * MMU management API.
 */

enum gcerror gcmmu_init(struct gccorecontext *gccorecontext)
{
	enum gcerror gcerror;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;

	GCENTER(GCZONE_INIT);

	/* Initialize access lock. */
	GCLOCK_INIT(&gcmmu->lock);

	/* Allocate one page. */
	gcerror = gc_alloc_noncached(&gcmmu->gcpage, PAGE_SIZE);
	if (gcerror != GCERR_NONE) {
		GCERR("failed to allocate MMU management buffer.\n");
		gcerror = GCERR_SETGRP(gcerror, GCERR_MMU_SAFE_ALLOC);
		goto exit;
	}

	/* Determine the location of the physical command buffer. */
	gcmmu->cmdbufphys = gcmmu->gcpage.physical;
	gcmmu->cmdbuflog = gcmmu->gcpage.logical;
	gcmmu->cmdbufsize = PAGE_SIZE - GCMMU_SAFE_ZONE_SIZE;

	/* Determine the location of the safe zone. */
	gcmmu->safezonephys = gcmmu->gcpage.physical + gcmmu->cmdbufsize;
	gcmmu->safezonelog = (unsigned int *) ((unsigned char *)
		gcmmu->gcpage.logical + gcmmu->cmdbufsize);
	gcmmu->safezonesize = GCMMU_SAFE_ZONE_SIZE;

	/* Reset the master table. */
	gcmmu->master = ~0U;

	/* Initialize the list of vacant arenas. */
	INIT_LIST_HEAD(&gcmmu->vacarena);

exit:
	GCEXITARG(GCZONE_INIT, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

void gcmmu_exit(struct gccorecontext *gccorecontext)
{
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct list_head *head;
	struct gcmmuarena *arena;

	GCENTER(GCZONE_INIT);

	/* Free the safe zone. */
	gc_free_noncached(&gcmmu->gcpage);

	/* Free vacant arena list. */
	while (!list_empty(&gcmmu->vacarena)) {
		head = gcmmu->vacarena.next;
		arena = list_entry(head, struct gcmmuarena, link);
		list_del(head);
		kfree(arena);
	}

	GCEXIT(GCZONE_INIT);
}

enum gcerror gcmmu_create_context(struct gccorecontext *gccorecontext,
				  struct gcmmucontext *gcmmucontext,
				  pid_t pid)
{
	enum gcerror gcerror;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct gcmmuarena *arena = NULL;
	unsigned int *logical;
	unsigned int i;

	GCENTER(GCZONE_CONTEXT);

	if (gcmmucontext == NULL) {
		gcerror = GCERR_MMU_CTXT_BAD;
		goto exit;
	}

	/* Reset the context. */
	memset(gcmmucontext, 0, sizeof(struct gcmmucontext));

	/* Initialize access lock. */
	GCLOCK_INIT(&gcmmucontext->lock);

	/* Initialize arena lists. */
	INIT_LIST_HEAD(&gcmmucontext->vacant);
	INIT_LIST_HEAD(&gcmmucontext->allocated);

	/* Mark context as dirty. */
	gcmmucontext->dirty = true;

	/* Set PID. */
	gcmmucontext->pid = pid;

	/* Allocate MTLB table. */
	gcerror = gc_alloc_cached(&gcmmucontext->master, GCMMU_MTLB_SIZE);
	if (gcerror != GCERR_NONE) {
		gcerror = GCERR_SETGRP(gcerror, GCERR_MMU_MTLB_ALLOC);
		goto exit;
	}

	/* Invalidate MTLB entries. */
	logical = gcmmucontext->master.logical;
	for (i = 0; i < GCMMU_MTLB_ENTRY_NUM; i += 1)
		logical[i] = GCMMU_MTLB_ENTRY_VACANT;

	/* Set MMU table mode. */
	gcmmucontext->mmuconfig.reg.master_mask
		= GCREG_MMU_CONFIGURATION_MASK_MODE_ENABLED;
	gcmmucontext->mmuconfig.reg.master = GCMMU_MTLB_MODE;

	/* Set the table address. */
	gcmmucontext->mmuconfig.reg.address_mask
		= GCREG_MMU_CONFIGURATION_MASK_ADDRESS_ENABLED;
	gcmmucontext->mmuconfig.reg.address
		= GCGETFIELD(gcmmucontext->master.physical,
			     GCREG_MMU_CONFIGURATION, ADDRESS);

	/* Allocate the first vacant arena. */
	gcerror = get_arena(gcmmu, &arena);
	if (gcerror != GCERR_NONE)
		goto exit;

	/* Entire range is currently vacant. */
	arena->start.absolute = 0;
	arena->end.absolute =
	arena->count = GCMMU_MTLB_ENTRY_NUM * GCMMU_STLB_ENTRY_NUM;
	list_add(&arena->link, &gcmmucontext->vacant);
	GCDUMPARENA(GCZONE_ARENA, "initial vacant arena", arena);

	/* Map the command queue. */
	gcerror = gcqueue_map(gccorecontext, gcmmucontext);
	if (gcerror != GCERR_NONE)
		goto exit;

	/* Reference MMU. */
	gcmmu->refcount += 1;

	GCEXIT(GCZONE_CONTEXT);
	return GCERR_NONE;

exit:
	gcmmu_destroy_context(gccorecontext, gcmmucontext);

	GCEXITARG(GCZONE_CONTEXT, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

enum gcerror gcmmu_destroy_context(struct gccorecontext *gccorecontext,
				   struct gcmmucontext *gcmmucontext)
{
	enum gcerror gcerror;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct list_head *head;
	struct gcmmuarena *arena;
	struct gcmmustlbblock *nextblock;

	GCENTER(GCZONE_CONTEXT);

	if (gcmmucontext == NULL) {
		gcerror = GCERR_MMU_CTXT_BAD;
		goto exit;
	}

	/* Unmap the command queue. */
	gcerror = gcqueue_unmap(gccorecontext, gcmmucontext);
	if (gcerror != GCERR_NONE)
		goto exit;

	/* Free allocated arenas. */
	while (!list_empty(&gcmmucontext->allocated)) {
		head = gcmmucontext->allocated.next;
		arena = list_entry(head, struct gcmmuarena, link);
		release_physical_pages(arena);
		list_move(head, &gcmmucontext->vacant);
	}

	/* Free slave tables. */
	while (gcmmucontext->slavealloc != NULL) {
		gc_free_cached(&gcmmucontext->slavealloc->pages);
		nextblock = gcmmucontext->slavealloc->next;
		kfree(gcmmucontext->slavealloc);
		gcmmucontext->slavealloc = nextblock;
	}

	/* Free the master table. */
	gc_free_cached(&gcmmucontext->master);

	/* Free arenas. */
	GCLOCK(&gcmmu->lock);
	list_splice_init(&gcmmucontext->vacant, &gcmmu->vacarena);
	GCUNLOCK(&gcmmu->lock);

	/* Dereference. */
	gcmmu->refcount -= 1;

	GCEXIT(GCZONE_CONTEXT);
	return GCERR_NONE;

exit:
	GCEXITARG(GCZONE_CONTEXT, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

enum gcerror gcmmu_enable(struct gccorecontext *gccorecontext,
			  struct gcqueue *gcqueue)
{
	enum gcerror gcerror;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct list_head *head;
	struct gccmdbuf *headcmdbuf;
	struct gccmdbuf *gccmdbuf = NULL;
	struct gcevent *gcevent = NULL;
	struct gcmommuinit *gcmommuinit;
	struct gcmosignal *gcmosignal;
	struct gccmdend *gccmdend;
	unsigned int status, enabled;

	GCENTER(GCZONE_INIT);

	/* Read the MMU status. */
	status = gc_read_reg(GCREG_MMU_CONTROL_Address);
	enabled = GCGETFIELD(status, GCREG_MMU_CONTROL, ENABLE);

	/* Is MMU enabled? */
	if (!enabled) {
		GCDBG(GCZONE_MASTER, "enabling MMU.\n");

		/* Queue cannot be empty. */
		if (list_empty(&gcqueue->queue)) {
			GCERR("queue is empty.");
			gcerror = GCERR_MMU_INIT;
			goto fail;
		}

		/* Get the first entry from the active queue. */
		head = gcqueue->queue.next;
		headcmdbuf = list_entry(head, struct gccmdbuf, link);

		/* Allocate command init buffer. */
		gcerror = gcqueue_alloc_cmdbuf(gcqueue, &gccmdbuf);
		if (gcerror != GCERR_NONE)
			goto fail;

		/* Add event for the current command buffer. */
		gcerror = gcqueue_alloc_event(gcqueue, &gcevent);
		if (gcerror != GCERR_NONE)
			goto fail;

		/* Get free interrupt. */
		gcerror = gcqueue_alloc_int(gcqueue, &gccmdbuf->interrupt);
		if (gcerror != GCERR_NONE)
			goto fail;

		/* Initialize the event and add to the list. */
		gcevent->handler = event_enable_mmu;

		/* Attach records. */
		list_add_tail(&gcevent->link, &gccmdbuf->events);
		list_add(&gccmdbuf->link, &gcqueue->queue);

		/* Program the safe zone and the master table address. */
		gcmommuinit = (struct gcmommuinit *) gcmmu->cmdbuflog;
		gcmommuinit->safe_ldst = gcmommuinit_safe_ldst;
		gcmommuinit->safe = gcmmu->safezonephys;
		gcmommuinit->mtlb = headcmdbuf->gcmmucontext->mmuconfig.raw;

		/* Configure EVENT command. */
		gcmosignal = (struct gcmosignal *) (gcmommuinit + 1);
		gcmosignal->signal_ldst = gcmosignal_signal_ldst;
		gcmosignal->signal.raw = 0;
		gcmosignal->signal.reg.id = gccmdbuf->interrupt;
		gcmosignal->signal.reg.pe = GCREG_EVENT_PE_SRC_ENABLE;

		/* Configure the END command. */
		gccmdend = (struct gccmdend *) (gcmosignal + 1);
		gccmdend->cmd.raw = gccmdend_const.cmd.raw;

		/* Initialize the command buffer. */
		gccmdbuf->gcmmucontext = headcmdbuf->gcmmucontext;
		gccmdbuf->logical = (unsigned char *) gcmmu->cmdbuflog;
		gccmdbuf->physical = gcmmu->cmdbufphys;
		gccmdbuf->size = sizeof(struct gcmommuinit)
			       + sizeof(struct gcmosignal)
			       + sizeof(struct gccmdend);
		gccmdbuf->count = (gccmdbuf->size + 7) >> 3;
		gccmdbuf->gcmoterminator = NULL;

		GCDUMPBUFFER(GCZONE_INIT, gccmdbuf->logical,
				gccmdbuf->physical, gccmdbuf->size);
	}

	GCEXIT(GCZONE_INIT);
	return GCERR_NONE;

fail:
	if (gcevent != NULL)
		gcqueue_free_event(gcqueue, gcevent);

	if (gccmdbuf != NULL)
		gcqueue_free_cmdbuf(gcqueue, gccmdbuf, NULL);

	GCEXITARG(GCZONE_CONTEXT, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

enum gcerror gcmmu_set_master(struct gccorecontext *gccorecontext,
			      struct gcmmucontext *gcmmucontext)
{
	enum gcerror gcerror;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct gcmommumaster *gcmommumaster;

	GCENTER(GCZONE_MASTER);

	if (gcmmucontext == NULL) {
		gcerror = GCERR_MMU_CTXT_BAD;
		goto exit;
	}

	/* Did the master change? */
	if (gcmmu->master == gcmmucontext->mmuconfig.raw) {
		gcerror = GCERR_NONE;
		goto exit;
	}

	/* Allocate command buffer space. */
	gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
				sizeof(struct gcmommumaster),
				(void **) &gcmommumaster, NULL);
	if (gcerror != GCERR_NONE) {
		gcerror = GCERR_SETGRP(gcerror, GCERR_MMU_MTLB_SET);
		goto exit;
	}

	/* Program master table address. */
	gcmommumaster->master_ldst = gcmommumaster_master_ldst;
	gcmommumaster->master = gcmmucontext->mmuconfig.raw;

	/* Update master table address. */
	gcmmu->master = gcmmucontext->mmuconfig.raw;

exit:
	GCEXITARG(GCZONE_MASTER, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

enum gcerror gcmmu_map(struct gccorecontext *gccorecontext,
		       struct gcmmucontext *gcmmucontext,
		       struct gcmmuphysmem *mem,
		       struct gcmmuarena **mapped)
{
	enum gcerror gcerror = GCERR_NONE;
	bool locked = false;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct list_head *arenahead;
	struct gcmmuarena *vacant = NULL, *split;
	struct gcmmustlb *slave;
	unsigned int *stlblogical;
	union gcmmuloc index;
	unsigned int i, allocated, count;
	pte_t *parray_alloc = NULL;
	pte_t *parray;

	GCENTER(GCZONE_MAPPING);

	if (gcmmucontext == NULL) {
		gcerror = GCERR_MMU_CTXT_BAD;
		goto exit;
	}

	if ((mem == NULL) || (mem->count <= 0) || (mapped == NULL) ||
		((mem->pagesize != 0) && (mem->pagesize != GCMMU_PAGE_SIZE))) {
		gcerror = GCERR_MMU_ARG;
		goto exit;
	}

	GCLOCK(&gcmmucontext->lock);
	locked = true;

	/*
	 * Find available sufficient arena.
	 */

	GCDBG(GCZONE_MAPPING, "mapping (%d) pages\n", mem->count);

	list_for_each(arenahead, &gcmmucontext->vacant) {
		vacant = list_entry(arenahead, struct gcmmuarena, link);
		if (vacant->count >= mem->count)
			break;
	}

	if (arenahead == &gcmmucontext->vacant) {
		gcerror = GCERR_MMU_OOM;
		goto exit;
	}

	GCDUMPARENA(GCZONE_ARENA, "allocating from arena", vacant);

	/*
	 * If page array isn't provided, create it here.
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
			goto exit;
		}

		/* Fetch page addresses. */
		gcerror = get_physical_pages(mem, parray_alloc, vacant);
		if (gcerror != GCERR_NONE)
			goto exit;

		parray = parray_alloc;

		GCDBG(GCZONE_MAPPING,
			"physical page array allocated (0x%08X)\n",
			(unsigned int) parray);
	} else {
		parray = mem->pages;

		GCDBG(GCZONE_MAPPING,
			"physical page array provided (0x%08X)\n",
			(unsigned int) parray);
	}

	/*
	 * Create the mapping.
	 */

	index.absolute = vacant->start.absolute;
	slave = &gcmmucontext->slave[index.loc.mtlb];
	count = mem->count;

	while (count > 0) {
		/* Allocate slaves if not yet allocated. */
		if (slave->logical == NULL) {
			gcerror = allocate_slave(gcmmucontext, index);
			if (gcerror != GCERR_NONE)
				goto exit;
		}

		/* Determine the number of entries allocated. */
		allocated = GCMMU_STLB_ENTRY_NUM - index.loc.stlb;
		if (allocated > count)
			allocated = count;

		/* Initialize slave entries. */
		stlblogical = &slave->logical[index.loc.stlb];
		for (i = 0; i < allocated; i += 1)
			*stlblogical++
				= (*parray++ & GCMMU_STLB_ADDRESS_MASK)
				| GCMMU_STLB_PRESENT
				| GCMMU_STLB_EXCEPTION
				| GCMMU_STLB_WRITEABLE;

		/* Flush CPU cache. */
		gc_flush_region(slave->physical, slave->logical,
				index.loc.stlb * sizeof(unsigned int),
				allocated * sizeof(unsigned int));

		GCDBG(GCZONE_MAPPING, "allocated %d pages at %d.%d\n",
		      allocated, index.loc.mtlb, index.loc.stlb);

		/* Advance. */
		slave += 1;
		index.absolute += allocated;
		count -= allocated;
	}

	/*
	 * Claim arena.
	 */

	/* Split the arena. */
	if (vacant->count != mem->count) {
		GCDBG(GCZONE_MAPPING, "splitting vacant arena\n");

		gcerror = get_arena(gcmmu, &split);
		if (gcerror != GCERR_NONE)
			goto exit;

		split->start.absolute = index.absolute;
		split->end.absolute = vacant->end.absolute;
		split->count = vacant->count - mem->count;
		list_add(&split->link, &vacant->link);

		vacant->end.absolute = index.absolute;
		vacant->count = mem->count;
	}

	GCDUMPARENA(GCZONE_ARENA, "allocated arena", vacant);

	/* Move the vacant arena to the list of allocated arenas. */
	list_move(&vacant->link, &gcmmucontext->allocated);

	/* Set page size. */
	mem->pagesize = GCMMU_PAGE_SIZE;

	/* Determine the virtual address. */
	vacant->address
		= ((vacant->start.loc.mtlb << GCMMU_MTLB_SHIFT)
					    & GCMMU_MTLB_MASK)
		| ((vacant->start.loc.stlb << GCMMU_STLB_SHIFT)
					    & GCMMU_STLB_MASK)
		| (mem->offset & GCMMU_OFFSET_MASK);

	/* Determine the size of the area. */
	vacant->size = mem->count * GCMMU_PAGE_SIZE - mem->offset;

	/* Invalidate the MMU. */
	gcmmucontext->dirty = true;

	/* Set the result. */
	*mapped = vacant;

	GCDBG(GCZONE_MAPPING, "mapped %d bytes at 0x%08X\n",
		vacant->size, vacant->address);

exit:
	if (parray_alloc != NULL) {
		kfree(parray_alloc);

		if (gcerror != GCERR_NONE)
			release_physical_pages(vacant);
	}

	if (locked)
		GCUNLOCK(&gcmmucontext->lock);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

enum gcerror gcmmu_unmap(struct gccorecontext *gccorecontext,
			 struct gcmmucontext *gcmmucontext,
			 struct gcmmuarena *mapped)
{
	enum gcerror gcerror = GCERR_NONE;
	bool locked = false;
	struct gcmmu *gcmmu = &gccorecontext->gcmmu;
	struct list_head *allochead, *prevhead, *nexthead;
	struct gcmmuarena *allocated, *prevvacant, *nextvacant = NULL;
	struct gcmmustlb *slave;
	unsigned int *stlblogical;
	union gcmmuloc index;
	unsigned int i, freed, count;

	GCENTER(GCZONE_MAPPING);

	if (gcmmucontext == NULL) {
		gcerror = GCERR_MMU_CTXT_BAD;
		goto exit;
	}

	GCLOCK(&gcmmucontext->lock);
	locked = true;

	/*
	 * Find the arena.
	 */

	GCDBG(GCZONE_MAPPING, "unmapping arena 0x%08X\n",
		(unsigned int) mapped);

	list_for_each(allochead, &gcmmucontext->allocated) {
		allocated = list_entry(allochead, struct gcmmuarena, link);
		if (allocated == mapped)
			break;
	}

	if (allochead == &gcmmucontext->allocated) {
		GCERR("mapped arena 0x%08X not found.\n",
			(unsigned int) mapped);
		gcerror = GCERR_MMU_ARG;
		goto exit;
	}

	GCDBG(GCZONE_MAPPING, "mapped arena found:\n");
	GCDBG(GCZONE_MAPPING, "  arena phys = 0x%08X\n", allocated->address);
	GCDBG(GCZONE_MAPPING, "  arena size = %d\n", allocated->size);

	/*
	 * Free slave tables.
	 */

	index.absolute = allocated->start.absolute;
	slave = &gcmmucontext->slave[index.loc.mtlb];
	count = allocated->count;

	while (count > 0) {
		/* Determine the number of entries freed. */
		freed = GCMMU_STLB_ENTRY_NUM - index.loc.stlb;
		if (freed > count)
			freed = count;

		GCDBG(GCZONE_MAPPING, "freeing %d pages at %d.%d\n",
			freed, index.loc.mtlb, index.loc.stlb);

		/* Free slave entries. */
		stlblogical = &slave->logical[index.loc.stlb];
		for (i = 0; i < freed; i += 1)
			*stlblogical++ = GCMMU_STLB_ENTRY_VACANT;

		/* Flush CPU cache. */
		gc_flush_region(slave->physical, slave->logical,
				index.loc.stlb * sizeof(unsigned int),
				freed * sizeof(unsigned int));

		/* Advance. */
		slave += 1;
		index.absolute += freed;
		count -= freed;
	}

	/*
	 * Delete page cache for the arena.
	 */

	release_physical_pages(allocated);

	/*
	 * Find point of insertion and free the arena.
	 */

	GCDBG(GCZONE_MAPPING,
		"looking for the point of insertion.\n");

	list_for_each(nexthead, &gcmmucontext->vacant) {
		nextvacant = list_entry(nexthead, struct gcmmuarena, link);
		if (nextvacant->start.absolute >= allocated->end.absolute) {
			GCDBG(GCZONE_MAPPING, "  point of insertion found.\n");
			break;
		}
	}

	/* Get the previous vacant entry. */
	prevhead = nexthead->prev;

	/* Merge the area back into vacant list. */
	if (siblings(&gcmmucontext->vacant, prevhead, allochead)) {
		if (siblings(&gcmmucontext->vacant, allochead, nexthead)) {
			prevvacant = list_entry(prevhead, struct gcmmuarena,
						link);

			GCDBG(GCZONE_MAPPING, "merging three arenas:\n");

			GCDUMPARENA(GCZONE_ARENA, "previous arena", prevvacant);
			GCDUMPARENA(GCZONE_ARENA, "allocated arena", allocated);
			GCDUMPARENA(GCZONE_ARENA, "next arena", nextvacant);

			/* Merge all three arenas. */
			prevvacant->count += allocated->count;
			prevvacant->count += nextvacant->count;
			prevvacant->end.absolute = nextvacant->end.absolute;

			/* Free the merged arenas. */
			GCLOCK(&gcmmu->lock);
			list_move(allochead, &gcmmu->vacarena);
			list_move(nexthead, &gcmmu->vacarena);
			GCUNLOCK(&gcmmu->lock);
		} else {
			prevvacant = list_entry(prevhead, struct gcmmuarena,
						link);

			GCDBG(GCZONE_MAPPING, "merging with the previous:\n");

			GCDUMPARENA(GCZONE_ARENA, "previous arena", prevvacant);
			GCDUMPARENA(GCZONE_ARENA, "allocated arena", allocated);

			/* Merge with the previous. */
			prevvacant->count += allocated->count;
			prevvacant->end.absolute = allocated->end.absolute;

			/* Free the merged arena. */
			GCLOCK(&gcmmu->lock);
			list_move(allochead, &gcmmu->vacarena);
			GCUNLOCK(&gcmmu->lock);
		}
	} else if (siblings(&gcmmucontext->vacant, allochead, nexthead)) {
		GCDBG(GCZONE_MAPPING, "merged with the next:\n");

		GCDUMPARENA(GCZONE_ARENA, "allocated arena", allocated);
		GCDUMPARENA(GCZONE_ARENA, "next arena", nextvacant);

		/* Merge with the next arena. */
		nextvacant->start.absolute = allocated->start.absolute;
		nextvacant->count += allocated->count;

		/* Free the merged arena. */
		GCLOCK(&gcmmu->lock);
		list_move(allochead, &gcmmu->vacarena);
		GCUNLOCK(&gcmmu->lock);
	} else {
		GCDBG(GCZONE_MAPPING,
		      "nothing to merge with, inserting in between:\n");
		GCDUMPARENA(GCZONE_ARENA, "allocated arena", allocated);

		/* Neighbor vacant arenas are not siblings, can't merge. */
		list_move(allochead, prevhead);
	}

	/* Invalidate the MMU. */
	gcmmucontext->dirty = true;

exit:
	if (locked)
		GCUNLOCK(&gcmmucontext->lock);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

enum gcerror gcmmu_flush(struct gccorecontext *gccorecontext,
			 struct gcmmucontext *gcmmucontext)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcmommuflush *flushlogical;
	unsigned int flushaddress;
	struct gcqueue *gcqueue;

	GCENTER(GCZONE_FLUSH);

	GCLOCK(&gcmmucontext->lock);

	if (gcmmucontext->dirty) {
		/* Allocate space for the flush. */
		gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
					sizeof(struct gcmommuflush),
					(void **) &flushlogical, &flushaddress);
		if (gcerror != GCERR_NONE)
			goto exit;

		/* Get a shortcut to the queue object. */
		gcqueue = &gccorecontext->gcqueue;

		/* Store flush information. */
		gcqueue->flushlogical = flushlogical;
		gcqueue->flushaddress = flushaddress;

		/* Validate the context. */
		gcmmucontext->dirty = false;
	}

exit:
	GCUNLOCK(&gcmmucontext->lock);

	GCEXITARG(GCZONE_FLUSH, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

void gcmmu_flush_finalize(struct gccmdbuf *gccmdbuf,
			  struct gcmommuflush *flushlogical,
			  unsigned int flushaddress)
{
	int size;
	unsigned int datacount;

	GCENTER(GCZONE_FLUSH);

	/* Compute the size of the command buffer after the flush block */
	size = gccmdbuf->physical + gccmdbuf->size
		- flushaddress - sizeof(struct gcmommuflush);

	/* Compute the data count. */
	datacount = (size + 7) >> 3;

	/* Flush 2D PE cache. */
	flushlogical->peflush.flush_ldst = gcmoflush_flush_ldst;
	flushlogical->peflush.flush.reg = gcregflush_pe2D;

	/* Arm the FE-PE semaphore. */
	flushlogical->peflushsema.sema_ldst = gcmosema_sema_ldst;
	flushlogical->peflushsema.sema.reg  = gcregsema_fe_pe;

	/* Stall FE until PE is done flushing. */
	flushlogical->peflushstall.cmd.fld = gcfldstall;
	flushlogical->peflushstall.arg.fld = gcfldstall_fe_pe;

	/* LINK to the next slot to flush FE FIFO. */
	flushlogical->feflush.cmd.fld = gcfldlink4;
	flushlogical->feflush.address
		= flushaddress
		+ offsetof(struct gcmommuflush, mmuflush_ldst);

	/* Flush MMU cache. */
	flushlogical->mmuflush_ldst = gcmommuflush_mmuflush_ldst;
	flushlogical->mmuflush.reg = gcregmmu_flush;

	/* Arm the FE-PE semaphore. */
	flushlogical->mmuflushsema.sema_ldst = gcmosema_sema_ldst;
	flushlogical->mmuflushsema.sema.reg  = gcregsema_fe_pe;

	/* Stall FE until PE is done flushing. */
	flushlogical->mmuflushstall.cmd.fld = gcfldstall;
	flushlogical->mmuflushstall.arg.fld = gcfldstall_fe_pe;

	/* LINK to the next slot to flush FE FIFO. */
	flushlogical->link.cmd.fld.opcode = GCREG_COMMAND_OPCODE_LINK;
	flushlogical->link.cmd.fld.count = datacount;
	flushlogical->link.address = flushaddress + sizeof(struct gcmommuflush);

	GCEXIT(GCZONE_FLUSH);
}

enum gcerror gcmmu_fixup(struct gcfixup *fixup, unsigned int *data)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcfixupentry *table;
	struct gcmmuarena *arena;
	unsigned int dataoffset;
	unsigned int i;

	GCENTER(GCZONE_FIXUP);

	/* Process fixups. */
	while (fixup != NULL) {
		/* Verify fixup pointer. */
		if (!virt_addr_valid(fixup)) {
			GCERR("bad fixup @ 0x%08X\n", (unsigned int) fixup);
			gcerror = GCERR_OODM;
			goto exit;
		}

		GCDBG(GCZONE_FIXUP, "%d fixup(s) @ 0x%08X\n",
			fixup->count, (unsigned int) fixup);

		/* Apply fixups. */
		table = fixup->fixup;
		for (i = 0; i < fixup->count; i += 1) {
			GCDBG(GCZONE_FIXUP, "#%d\n", i);
			GCDBG(GCZONE_FIXUP, "  buffer offset = 0x%08X\n",
				table->dataoffset * 4);
			GCDBG(GCZONE_FIXUP, "  surface offset = 0x%08X\n",
				table->surfoffset);

			dataoffset = table->dataoffset;

			arena = (struct gcmmuarena *) data[dataoffset];
			if (!virt_addr_valid(arena)) {
				GCERR("bad arena @ 0x%08X\n",
				      (unsigned int) arena);
				gcerror = GCERR_OODM;
				goto exit;
			}

			GCDBG(GCZONE_FIXUP, "  arena = 0x%08X\n",
				(unsigned int) arena);
			GCDBG(GCZONE_FIXUP, "  arena phys = 0x%08X\n",
				arena->address);
			GCDBG(GCZONE_FIXUP, "  arena size = %d\n",
				arena->size);

			data[dataoffset] = arena->address + table->surfoffset;

			GCDBG(GCZONE_FIXUP, "  surface @ 0x%08X\n",
				data[dataoffset]);

			table += 1;
		}

		/* Get the next fixup. */
		fixup = fixup->next;
	}

exit:
	GCEXITARG(GCZONE_FIXUP, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}
