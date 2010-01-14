/*
 * dmm_page_rep.c
 *
 * DMM driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/mm.h>
#include <linux/mmzone.h>
#include <asm/cacheflush.h>
#include <linux/mutex.h>

#include "dmm_page_rep.h"
#include "../tiler/tiler_deprecate.h"

static struct mutex mtx;

#ifdef CHECK_STACK
#define lajosdump(x) printk(KERN_NOTICE "%s::%s():%d: %s=%p\n",\
					__FILE__, __func__, __LINE__, #x, x);
#else
#define lajosdump(x)
#endif

unsigned long freePageCnt;
unsigned char *dmmPhysPages;
struct dmmPhysPgLLT *freePagesStack;
struct dmmPhysPgLLT *usedPagesStack;

#ifdef CHECK_STACK
static void print_stack(struct dmmPhysPgLLT *stack, char *prefix, int line)
{
	while (stack) {
		printk(KERN_ERR "%s%p<-%p->%p", prefix, stack->prevPhysPg,
						stack, stack->nextPhysPg);
		stack = stack->prevPhysPg;
		prefix = "";
	}
	printk(KERN_ERR "in line %d.", line);
}

static void check_stack(struct dmmPhysPgLLT *stack, char *prefix, int line)
{
	if (!stack)
		return;
	if (stack->nextPhysPg != NULL)
		print_stack(stack, prefix, line);
	else {
		struct dmmPhysPgLLT *ptr = stack;
		while (ptr) {
			if (ptr->prevPhysPg != NULL &&
					ptr->prevPhysPg->nextPhysPg != ptr) {
				print_stack(stack, prefix, line);
				return;
			}
			ptr = ptr->prevPhysPg;
		}
	}
}
#endif

/* ========================================================================== */
/**
 *  dmmPhysPageRepRefil()
 *
 * @brief  Refills the physical page repository with the predefined page amount.
 *
 * @return none
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum errorCodeT dmm_phys_page_rep_refil(void)
{
	unsigned long iter;
	struct page *page = NULL;

	for (iter = 0; iter < DMM_MNGD_PHYS_PAGES; iter++) {
		struct dmmPhysPgLLT *tmpPgNode = NULL;

		tmpPgNode = kmalloc(sizeof(struct dmmPhysPgLLT), GFP_KERNEL);
		if (!tmpPgNode)
			return DMM_SYS_ERROR;
		memset(tmpPgNode, 0x0, sizeof(struct dmmPhysPgLLT));

		if (tmpPgNode != NULL) {
			tmpPgNode->nextPhysPg = NULL;
			tmpPgNode->prevPhysPg = NULL;
			page = (struct page *)alloc_page(GFP_KERNEL | GFP_DMA);
			if (!page)
				return DMM_SYS_ERROR;
			tmpPgNode->physPgPtr =
					(unsigned long *)page_to_phys(page);
			tmpPgNode->page_addr = page;
			dmac_flush_range((void *)page_address(page),
					(void *)page_address(page) + 0x1000);
			outer_flush_range((unsigned long)tmpPgNode->physPgPtr,
				(unsigned long)tmpPgNode->physPgPtr +  0x1000);

			/* add to end */
			if (freePagesStack != NULL) {
				if (freePagesStack->nextPhysPg != NULL)
					lajosdump(freePagesStack->nextPhysPg);
				freePagesStack->nextPhysPg = tmpPgNode;
				tmpPgNode->prevPhysPg = freePagesStack;
			}

			freePagesStack = tmpPgNode;
			freePageCnt++;
		} else {
			printk(KERN_ERR "error\n");
		}
	}

#ifdef CHECK_STACK
	check_stack(freePagesStack, "free: ", __LINE__);
#endif
	return DMM_NO_ERROR;
}

/* ========================================================================== */
/**
 *  dmmPhysPageRepInit()
 *
 * @brief  Initializes the physical memory page repository instance.
 *
 * @return errorCodeT
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see errorCodeT for further detail.
 */
/* ========================================================================== */
s32 dmm_phys_page_rep_init(void)
{
	int r = DMM_SYS_ERROR;
	mutex_init(&mtx);
	mutex_lock(&mtx);

	freePagesStack = NULL;
	usedPagesStack = NULL;
	freePageCnt = 0;

	r = dmm_phys_page_rep_refil();

	mutex_unlock(&mtx);
	return r;
}
EXPORT_SYMBOL(dmm_phys_page_rep_init);

/* ========================================================================== */
/**
 *  dmmPhysPageRepDeinit()
 *
 * @brief  Releases all resources held by the physical memory page repository
 * instance.
 *
 * @return errorCodeT
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see errorCodeT for further detail.
 */
/* ========================================================================== */
s32 dmm_phys_page_rep_deinit(void)
{
	struct dmmPhysPgLLT *tmpPgNode = NULL;

	mutex_lock(&mtx);

	while (usedPagesStack != NULL) {
		tmpPgNode = usedPagesStack->prevPhysPg;
		__free_page(usedPagesStack->page_addr);
		kfree(usedPagesStack);
		usedPagesStack = tmpPgNode;
	}

	while (freePagesStack != NULL) {
		tmpPgNode = freePagesStack->prevPhysPg;
		__free_page(freePagesStack->page_addr);
		kfree(freePagesStack);
		freePagesStack = tmpPgNode;
	}

	freePageCnt = 0;

	mutex_unlock(&mtx);
	mutex_destroy(&mtx);
	return DMM_NO_ERROR;
}
EXPORT_SYMBOL(dmm_phys_page_rep_deinit);

/* ========================================================================== */
/**
 *  dmmGetPhysPage()
 *
 * @brief  Return a pointer to a physical memory page and mark it as used.
 *
 * @return MSP_U32* pointer to a physical memory page, NULL if error occurs.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
u32 dmm_get_phys_page(void)
{
	unsigned long *physPgPtr = NULL;
	int r;

	mutex_lock(&mtx);

	if (freePagesStack == NULL) {
		r = dmm_phys_page_rep_refil();
		if (r != DMM_NO_ERROR)
			return NULL;
	}

	if (freePagesStack != NULL) {
		struct dmmPhysPgLLT *tmpPgNode = freePagesStack;

		/* remove page from top of freepages */
		if (freePagesStack->nextPhysPg != NULL)
			lajosdump(freePagesStack->nextPhysPg);
		freePagesStack = freePagesStack->prevPhysPg;
		if (freePagesStack != NULL)
			freePagesStack->nextPhysPg = NULL;

		/* add page to top of usedpages */
		tmpPgNode->prevPhysPg = usedPagesStack;
		if (usedPagesStack != NULL) {
			if (usedPagesStack->nextPhysPg != NULL)
				lajosdump(usedPagesStack->nextPhysPg);
			usedPagesStack->nextPhysPg = tmpPgNode;
		}
		tmpPgNode->nextPhysPg = NULL;
		usedPagesStack = tmpPgNode;

		physPgPtr = tmpPgNode->physPgPtr;
		freePageCnt--;
	}

#ifdef CHECK_STACK
	check_stack(freePagesStack, "free: ", __LINE__);
	check_stack(usedPagesStack, "used: ", __LINE__);
#endif
	mutex_unlock(&mtx);
	return (u32)physPgPtr;
}
EXPORT_SYMBOL(dmm_get_phys_page);

/* ========================================================================== */
/**
 *  dmmFreePhysPage()
 *
 * @brief  Frees a specified physical memory page.
 *
 * @param physPgPtr - MSP_U32* - [in] The address of the allocated physical page
 * that should be freed.
 *
 * @return errorCodeT
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see errorCodeT for further detail.
 */
/* ========================================================================== */
void dmm_free_phys_page(u32 page_addr)
{
	struct dmmPhysPgLLT *iterPgNode = usedPagesStack;

	mutex_lock(&mtx);

	while (iterPgNode != NULL) {

		if (iterPgNode->physPgPtr == page_addr) {
			/* find address in used pages stack */

			/* remove from list */
			if (iterPgNode->prevPhysPg != NULL) {
				iterPgNode->prevPhysPg->nextPhysPg =
					iterPgNode->nextPhysPg;
			}

			if (iterPgNode->nextPhysPg != NULL) {
				iterPgNode->nextPhysPg->prevPhysPg =
					iterPgNode->prevPhysPg;
			} else if (iterPgNode == usedPagesStack) {
				usedPagesStack = usedPagesStack->prevPhysPg;
			} else {
				mutex_unlock(&mtx);
				lajosdump(iterPgNode);
				//return DMM_SYS_ERROR;
			}

			/* add to end of freepages */
			if (freePagesStack != NULL)
				freePagesStack->nextPhysPg = iterPgNode;
			iterPgNode->prevPhysPg = freePagesStack;
			freePagesStack = iterPgNode;
			freePageCnt++;

			while (freePageCnt > DMM_MNGD_PHYS_PAGES &&
						freePagesStack != NULL) {
				iterPgNode = freePagesStack->prevPhysPg;
				__free_page(freePagesStack->page_addr);
				kfree(freePagesStack);
				freePagesStack = iterPgNode;
				freePageCnt--;
			}
			freePagesStack->nextPhysPg = NULL;

			mutex_unlock(&mtx);
#ifdef CHECK_STACK
			check_stack(freePagesStack, "free: ", __LINE__);
			check_stack(usedPagesStack, "used: ", __LINE__);
#endif
			//return DMM_NO_ERROR;
		}

		iterPgNode = iterPgNode->prevPhysPg;
	}

	mutex_unlock(&mtx);
#ifdef CHECK_STACK
	check_stack(freePagesStack, "free: ", __LINE__);
	check_stack(usedPagesStack, "used: ", __LINE__);
#endif
	//return DMM_WRONG_PARAM;
}
EXPORT_SYMBOL(dmm_free_phys_page);

