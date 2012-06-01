/*
 * drivers/video/omap2/dsscomp/tiler-utils.c
 *
 * Tiler Utility APIs source file
 *
 * Copyright (C) 2011 Texas Instruments, Inc
 * Author: Sreenidhi Koti <sreenidhi@ti.com>
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

#include <linux/shmem_fs.h>
#include <linux/module.h>

#include "tiler-utils.h"
/*  ==========================================================================
 *  Kernel APIs
 *  ==========================================================================
 */

u32 tiler_virt2phys(u32 usr)
{
	pmd_t *pmd;
	pte_t *ptep;
	pgd_t *pgd = pgd_offset(current->mm, usr);

	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return 0;

	pmd = pmd_offset((pud_t *)pgd, usr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return 0;

	ptep = pte_offset_map(pmd, usr);
	if (ptep && pte_present(*ptep))
		return (*ptep & PAGE_MASK) | (~PAGE_MASK & usr);

	return 0;
}

void tiler_pa_free(struct tiler_pa_info *pa)
{
	if (pa)
		kfree(pa->mem);
	kfree(pa);
}
EXPORT_SYMBOL(tiler_pa_free);

/* get physical pages of a user block */
struct tiler_pa_info *user_block_to_pa(u32 usr_addr, u32 num_pg)
{
	struct task_struct *curr_task = current;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;

	struct tiler_pa_info *pa = NULL;
	struct page **pages = NULL;
	u32 *mem = NULL, write, i;
	int usr_count;

	pa = kzalloc(sizeof(*pa), GFP_KERNEL);
	if (!pa)
		return NULL;

	mem = kzalloc(num_pg * sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		kfree(pa);
		return NULL;
	}

	pages = kmalloc(num_pg * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		kfree(mem);
		kfree(pa);
		return NULL;
	}

	/*
	 * Important Note: usr_addr is mapped from user
	 * application process to current process - it must lie
	 * completely within the current virtual memory address
	 * space in order to be of use to us here.
	 */
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, usr_addr + (num_pg << PAGE_SHIFT) - 1);

	if (!vma || (usr_addr < vma->vm_start)) {
		kfree(mem);
		kfree(pa);
		kfree(pages);
		up_read(&mm->mmap_sem);
		printk(KERN_ERR "Address is outside VMA: ");
		printk(KERN_ERR "address start = %08x", usr_addr);
		printk(KERN_ERR "user end = %08x\n", (usr_addr +
					(num_pg << PAGE_SHIFT)));
		return ERR_PTR(-EFAULT);
	}

	if (vma->vm_flags & (VM_WRITE | VM_MAYWRITE))
		write = 1;

	usr_count = get_user_pages(curr_task, mm, usr_addr, num_pg, write, 1,
					pages, NULL);

	if (usr_count > 0) {
		/* process user allocated buffer */
		if (usr_count != num_pg) {
			/* release the pages we did get */
			for (i = 0; i < usr_count; i++)
				page_cache_release(pages[i]);
		} else {
			/* fill in the physical address information */
			for (i = 0; i < num_pg; i++) {
				mem[i] = page_to_phys(pages[i]);
				BUG_ON(pages[i] != phys_to_page(mem[i]));
			}
		}
	} else {
		/* fallback for kernel allocated buffers */
		for (i = 0; i < num_pg; i++) {
			mem[i] = tiler_virt2phys(usr_addr);

			if (!mem[i]) {
				printk(KERN_ERR "VMA not in page table\n");
				break;
			}

			usr_addr += PAGE_SIZE;
		}
	}

	up_read(&mm->mmap_sem);

	kfree(pages);

	/* if failed to map all pages */
	if (i < num_pg) {
		kfree(mem);
		kfree(pa);
		return ERR_PTR(-EFAULT);
	}

	pa->mem = mem;
	pa->memtype = usr_count > 0 ? TILER_MEM_GOT_PAGES : TILER_MEM_USING;
	pa->num_pg = num_pg;
	return pa;
}

