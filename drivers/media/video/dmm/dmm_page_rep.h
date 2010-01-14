#ifndef DMM_PAGE_REP_H
#define DMM_PAGE_REP_H

#define DMM_MNGD_PHYS_PAGES (16)

/** @struc dmmPhysPgLLT
* Structure defining Dmm physical memory pages managment linked list. */
struct dmmPhysPgLLT {
	struct dmmPhysPgLLT *nextPhysPg;
	struct dmmPhysPgLLT *prevPhysPg;
	unsigned long *physPgPtr;
	struct page *page_addr;
};

s32 dmm_phys_page_rep_init(void);
s32 dmm_phys_page_rep_deinit(void);
u32 dmm_get_phys_page(void);
void dmm_free_phys_page(u32 page_addr);

#endif

