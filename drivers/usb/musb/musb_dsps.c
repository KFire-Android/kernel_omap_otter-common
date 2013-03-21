/*
 * Texas Instruments DSPS platforms "glue layer"
 *
 * Copyright (C) 2012, by Texas Instruments
 *
 * Based on the am35x "glue layer" code.
 *
 * This file is part of the Inventra Controller Driver for Linux.
 *
 * The Inventra Controller Driver for Linux is free software; you
 * can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * The Inventra Controller Driver for Linux is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with The Inventra Controller Driver for Linux ; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA
 *
 * musb_dsps.c will be a common file for all the TI DSPS platforms
 * such as dm64x, dm36x, dm35x, da8x, am35x and ti81x.
 * For now only ti81x is using this and in future davinci.c, am35x.c
 * da8xx.c would be merged to this file after testing.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/usb/nop-usb-xceiv.h>
#include <linux/platform_data/usb-omap.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "musb_core.h"
#ifdef CONFIG_USB_TI_CPPI41_DMA
#include "cppi41.h"
#include "cppi41_dma.h"
#endif

#ifdef CONFIG_OF
static const struct of_device_id musb_dsps_of_match[];
#endif

/**
 * avoid using musb_readx()/musb_writex() as glue layer should not be
 * dependent on musb core layer symbols.
 */
static inline u8 dsps_readb(const void __iomem *addr, unsigned offset)
	{ return __raw_readb(addr + offset); }

static inline u32 dsps_readl(const void __iomem *addr, unsigned offset)
	{ return __raw_readl(addr + offset); }

static inline void dsps_writeb(void __iomem *addr, unsigned offset, u8 data)
	{ __raw_writeb(data, addr + offset); }

static inline void dsps_writel(void __iomem *addr, unsigned offset, u32 data)
	{ __raw_writel(data, addr + offset); }

/**
 * DSPS usbss wrapper register offset
 * this register definition includes usbss wrapper for TI DSPS
 */
struct dsps_usbss_wrapper {
	u16	revision;
	u16	syscfg;
	u16	irq_eoi;
	u16	irq_status_raw;
	u16	irq_status;
	u16	irq_enable_set;
	u16	irq_enable_clr;
	u16	irq_tx0_dma_th_enb;
	u16	irq_rx0_dma_th_enb;
	u16	irq_tx1_dma_th_enb;
	u16	irq_rx1_dma_th_enb;
	u16	irq_usb0_dma_enable;
	u16	irq_usb1_dma_enable;
	u16	irq_tx0_frame_th_enb;
	u16	irq_rx0_frame_th_enb;
	u16	irq_tx1_frame_th_enb;
	u16	irq_rx1_frame_th_enb;
	u16	irq_usb0_frame_enable;
	u16	irq_usb1_frame_enable;
};

/**
 * DSPS musb wrapper register offset.
 * FIXME: This should be expanded to have all the wrapper registers from TI DSPS
 * musb ips.
 */
struct dsps_musb_wrapper {
	u16	revision;
	u16	control;
	u16	status;
	u16	eoi;
	u16	epintr_set;
	u16	epintr_clear;
	u16	epintr_status;
	u16	coreintr_set;
	u16	coreintr_clear;
	u16	coreintr_status;
#ifdef CONFIG_USB_TI_CPPI41_DMA
	u16	tx_mode;
	u16	rx_mode;
	u16	g_rndis;
	u16	auto_req;
	u16	tear_down;
	u16	th_xdma_idle;
#endif
	u16	srp_fix_time;
	u16	phy_utmi;
	u16	mgc_utmi_lpback;
	u16	mode;

	/* bit positions for control */
	unsigned	reset:5;

	/* bit positions for interrupt */
	unsigned	usb_shift:5;
	u32		usb_mask;
	u32		usb_bitmap;
	unsigned	drvvbus:5;

	unsigned	txep_shift:5;
	u32		txep_mask;
	u32		txep_bitmap;

	unsigned	rxep_shift:5;
	u32		rxep_mask;
	u32		rxep_bitmap;

	/* bit positions for phy_utmi */
	unsigned	otg_disable:5;

	/* bit positions for mode */
	unsigned	iddig:5;
	/* miscellaneous stuff */
	u32		musb_core_offset;
	u8		poll_seconds;
	/* number of musb instances */
	u8		instances;
	/* usbss wrapper register */
	struct dsps_usbss_wrapper usbss;
};

#ifdef CONFIG_PM_SLEEP
struct dsps_usbss_regs {
	u32	sysconfig;

	u32	irq_en_set;

#ifdef CONFIG_USB_TI_CPPI41_DMA
	u32	irq_dma_th_tx0[4];
	u32	irq_dma_th_rx0[4];
	u32	irq_dma_th_tx1[4];
	u32	irq_dma_th_rx1[4];
	u32	irq_dma_en[2];

	u32	irq_frame_th_tx0[4];
	u32	irq_frame_th_rx0[4];
	u32	irq_frame_th_tx1[4];
	u32	irq_frame_th_rx1[4];
	u32	irq_frame_en[2];
#endif
};

struct dsps_usb_regs {
	u32	control;

	u32	irq_en_set[2];

#ifdef CONFIG_USB_TI_CPPI41_DMA
	u32	tx_mode;
	u32	rx_mode;
	u32	grndis_size[15];
	u32	auto_req;
	u32	teardn;
	u32	th_xdma_idle;
#endif
	u32	srp_fix;
	u32	phy_utmi;
	u32	mgc_utmi_loopback;
	u32	mode;
};
#endif

/**
 * DSPS glue structure.
 */
struct dsps_glue {
	struct device *dev;
	struct platform_device *musb[2];	/* child musb pdev */
	const struct dsps_musb_wrapper *wrp; /* wrapper register offsets */
	struct timer_list timer[2];	/* otg_workaround timer */
	unsigned long last_timer[2];    /* last timer data for each instance */
	u32 __iomem *usb_ctrl[2];
	u32 __iomem *usbss_addr;
	u8	first;			/* ignore first call of resume */
#ifdef CONFIG_PM
	struct dsps_usbss_regs usbss_regs;
	struct dsps_usb_regs usb_regs[2];
#endif
#ifdef CONFIG_USB_TI_CPPI41_DMA
	struct cppi41_sched_tbl_t *dma_sched_table;
	u32 __iomem *dma_addr;
	u8 dma_irq;
	u8 dma_init_done;
	u8 max_dma_channel;
#endif
};

#define	DSPS_AM33XX_CONTROL_MODULE_PHYS_0	0x44e10620
#define	DSPS_AM33XX_CONTROL_MODULE_PHYS_1	0x44e10628

static const resource_size_t dsps_control_module_phys[] = {
	DSPS_AM33XX_CONTROL_MODULE_PHYS_0,
	DSPS_AM33XX_CONTROL_MODULE_PHYS_1,
};

#define USBPHY_CM_PWRDN		(1 << 0)
#define USBPHY_OTG_PWRDN	(1 << 1)
#define USBPHY_OTGVDET_EN	(1 << 19)
#define USBPHY_OTGSESSEND_EN	(1 << 20)

#ifdef CONFIG_USB_TI_CPPI41_DMA
static irqreturn_t cppi41dma_interrupt(int irq, void *hci);
#define CPPI41_ADDR(offs) ((void *)((u32)glue->dma_addr + (offs - 0x2000)))

/*
 * CPPI 4.1 resources used for USB OTG controller module:
 *
 tx/rx completion queues for usb0 */
static u16 tx_comp_q[] = {93, 94, 95, 96, 97,
				98, 99, 100, 101, 102,
				103, 104, 105, 106, 107 };

static u16 rx_comp_q[] = {109, 110, 111, 112, 113,
				114, 115, 116, 117, 118,
				119, 120, 121, 122, 123 };

/* tx/rx completion queues for usb1 */
static u16 tx_comp_q1[] = {125, 126, 127, 128, 129,
				 130, 131, 132, 133, 134,
				 135, 136, 137, 138, 139 };

static u16 rx_comp_q1[] = {141, 142, 143, 144, 145,
				 146, 147, 148, 149, 150,
				 151, 152, 153, 154, 155 };

/* cppi41 dma tx channel info */
static const struct cppi41_tx_ch tx_ch_info[] = {
	[0] = {
		.port_num	= 1,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 32} , {0, 33} }
	},
	[1] = {
		.port_num	= 2,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 34} , {0, 35} }
	},
	[2] = {
		.port_num	= 3,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 36} , {0, 37} }
	},
	[3] = {
		.port_num	= 4,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 38} , {0, 39} }
	},
	[4] = {
		.port_num	= 5,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 40} , {0, 41} }
	},
	[5] = {
		.port_num	= 6,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 42} , {0, 43} }
	},
	[6] = {
		.port_num	= 7,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 44} , {0, 45} }
	},
	[7] = {
		.port_num	= 8,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 46} , {0, 47} }
	},
	[8] = {
		.port_num	= 9,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 48} , {0, 49} }
	},
	[9] = {
		.port_num	= 10,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 50} , {0, 51} }
	},
	[10] = {
		.port_num	= 11,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 52} , {0, 53} }
	},
	[11] = {
		.port_num	= 12,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 54} , {0, 55} }
	},
	[12] = {
		.port_num	= 13,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 56} , {0, 57} }
	},
	[13] = {
		.port_num	= 14,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 58} , {0, 59} }
	},
	[14] = {
		.port_num	= 15,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 60} , {0, 61} }
	},
	[15] = {
		.port_num	= 1,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 62} , {0, 63} }
	},
	[16] = {
		.port_num	= 2,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 64} , {0, 65} }
	},
	[17] = {
		.port_num	= 3,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 66} , {0, 67} }
	},
	[18] = {
		.port_num	= 4,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 68} , {0, 69} }
	},
	[19] = {
		.port_num	= 5,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 70} , {0, 71} }
	},
	[20] = {
		.port_num	= 6,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 72} , {0, 73} }
	},
	[21] = {
		.port_num	= 7,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 74} , {0, 75} }
	},
	[22] = {
		.port_num	= 8,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 76} , {0, 77} }
	},
	[23] = {
		.port_num	= 9,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 78} , {0, 79} }
	},
	[24] = {
		.port_num	= 10,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 80} , {0, 81} }
	},
	[25] = {
		.port_num	= 11,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 82} , {0, 83} }
	},
	[26] = {
		.port_num	= 12,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 84} , {0, 85} }
	},
	[27] = {
		.port_num	= 13,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 86} , {0, 87} }
	},
	[28] = {
		.port_num	= 14,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 88} , {0, 89} }
	},
	[29] = {
		.port_num	= 15,
		.num_tx_queue	= 2,
		.tx_queue	= { {0, 90} , {0, 91} }
	}
};

/* Queues 0 to 66 are pre-assigned, others are spare */
static const u32 assigned_queues[] = {	0xffffffff, /* queue 0..31 */
					0xffffffff, /* queue 32..63 */
					0xffffffff, /* queue 64..95 */
					0xffffffff, /* queue 96..127 */
					0x0fffffff  /* queue 128..155 */
					};

#define USB_CPPI41_NUM_CH	15
#define USB_CPPI41_CH_NUM_PD	128
#define DSPS_TX_MODE_REG	0x70	/* Transparent, CDC, [Generic] RNDIS */
#define DSPS_RX_MODE_REG	0x74	/* Transparent, CDC, [Generic] RNDIS */
#define DSPS_USB_AUTOREQ_REG	0xd0
#define DSPS_USB_TEARDOWN_REG	0xd8
#define USB_CPPI41_MAX_PD	(USB_CPPI41_CH_NUM_PD * (USB_CPPI41_NUM_CH+1))

static int cppi41_init(struct dsps_glue *glue)
{
	struct usb_cppi41_info *cppi_info = &usb_cppi41_info[0];
	u16 blknum = 0, order;
	int i, id, status = 0, irq = glue->dma_irq;

	if (glue->dma_init_done)
		goto err0;

	for (id = 0; id < glue->wrp->instances; ++id) {
		cppi_info = &usb_cppi41_info[id];

		cppi_info->max_dma_ch = USB_CPPI41_NUM_CH;
		cppi_info->max_pkt_desc = USB_CPPI41_MAX_PD;

		cppi_info->ep_dma_ch = kzalloc(USB_CPPI41_NUM_CH, GFP_KERNEL);
		if (!cppi_info->ep_dma_ch) {
			pr_err("memory allocation failure\n");
			status = -ENOMEM;
			goto err1;
		}

		/* init cppi info structure  */
		cppi_info->dma_block = 0;
		for (i = 0 ; i < USB_CPPI41_NUM_CH ; i++)
			cppi_info->ep_dma_ch[i] = i + (15 * id);

		cppi_info->q_mgr = 0;
		cppi_info->num_tx_comp_q = 15;
		cppi_info->num_rx_comp_q = 15;
		cppi_info->tx_comp_q = id ? tx_comp_q1 : tx_comp_q;
		cppi_info->rx_comp_q = id ? rx_comp_q1 : rx_comp_q;
		cppi_info->bd_intr_enb = 1;
		cppi_info->rx_dma_mode = USB_TRANSPARENT_MODE;
		cppi_info->rx_inf_mode = 0;
		cppi_info->sched_tbl_ctrl = 0;

		cppi_info->wrp.autoreq_reg = DSPS_USB_AUTOREQ_REG;
		cppi_info->wrp.teardown_reg = DSPS_USB_TEARDOWN_REG;
		cppi_info->wrp.tx_mode_reg = DSPS_TX_MODE_REG;
		cppi_info->wrp.rx_mode_reg = DSPS_RX_MODE_REG;
	}

	glue->max_dma_channel = USB_CPPI41_NUM_CH * glue->wrp->instances * 2;
	glue->dma_sched_table = kzalloc(sizeof(struct cppi41_sched_tbl_t *) *
					glue->max_dma_channel, GFP_KERNEL);
	if (!glue->dma_sched_table) {
		pr_err("memory allocation failure\n");
		status = -ENOMEM;
		goto err1;
	}

	/* initialize the schedular table entries */
	for (i = 0; i < glue->max_dma_channel; i += 2) {
		/* add tx dmach for schedular table */
		glue->dma_sched_table[i].dma_ch = i/2;
		glue->dma_sched_table[i].is_tx = 1;
		glue->dma_sched_table[i].enb = 1;

		/* add rx dmach for schedular table */
		glue->dma_sched_table[i+1].dma_ch = i/2;
		glue->dma_sched_table[i+1].is_tx = 0;
		glue->dma_sched_table[i+1].enb = 1;
	}

	/* Queue manager information */
	cppi41_queue_mgr[0].num_queue = 159;
	cppi41_queue_mgr[0].queue_types = CPPI41_FREE_DESC_BUF_QUEUE |
						CPPI41_UNASSIGNED_QUEUE;
	cppi41_queue_mgr[0].base_fdbq_num = 0;
	cppi41_queue_mgr[0].assigned = assigned_queues;

	/* init DMA block */
	cppi41_dma_block[0].num_tx_ch = 30;
	cppi41_dma_block[0].num_rx_ch = 30;
	cppi41_dma_block[0].tx_ch_info = tx_ch_info;

	/* initilize cppi41 dma & Qmgr address */
	cppi41_queue_mgr[0].q_mgr_rgn_base = CPPI41_ADDR(QMGR_RGN_OFFS);
	cppi41_queue_mgr[0].desc_mem_rgn_base = CPPI41_ADDR(QMRG_DESCRGN_OFFS);
	cppi41_queue_mgr[0].q_mgmt_rgn_base = CPPI41_ADDR(QMGR_REG_OFFS);
	cppi41_queue_mgr[0].q_stat_rgn_base = CPPI41_ADDR(QMGR_STAT_OFFS);
	cppi41_dma_block[0].global_ctrl_base = CPPI41_ADDR(DMA_GLBCTRL_OFFS);
	cppi41_dma_block[0].ch_ctrl_stat_base = CPPI41_ADDR(DMA_CHCTRL_OFFS);
	cppi41_dma_block[0].sched_ctrl_base = CPPI41_ADDR(DMA_SCHED_OFFS);
	cppi41_dma_block[0].sched_table_base = CPPI41_ADDR(DMA_SCHEDTBL_OFFS);

	/* Initialize for Linking RAM region 0 alone */
	status = cppi41_queue_mgr_init(cppi_info->q_mgr, 0, 0x3fff);
	if (status < 0)
		goto err1;

	order = get_count_order(glue->max_dma_channel);
	if (order < 5)
		order = 5;

	status = cppi41_dma_block_init(blknum, cppi_info->q_mgr, order,
			glue->dma_sched_table, glue->max_dma_channel);
	if (status < 0)
		goto err1;

	/* attach to the IRQ */
	if (request_irq(irq, cppi41dma_interrupt, 0, "usb-cppi41dma", glue)) {
		pr_err("cppi41dma request_irq %d failed!\n", irq);
		status = -ENODEV;
		goto err1;
	} else
		pr_info("registerd cppi41-dma at IRQ %d dmabase %p\n",
			irq, cppi41_dma_block[0].global_ctrl_base);


	/* enable all usbss the interrupts */
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_eoi, 0);
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_enable_set,
		USBSS_INTR_FLAGS);
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_usb0_dma_enable,
		0xFFFeFFFe);

	glue->dma_init_done = 1;
	return 0;

err1:
	for (id = 0; id < glue->wrp->instances; ++id) {
		cppi_info = &usb_cppi41_info[id];
		kfree(cppi_info->ep_dma_ch);
	}
err0:
	return status;
}

void cppi41_free(struct dsps_glue *glue)
{
	u32 numch, blknum, order;
	struct usb_cppi41_info *cppi_info = &usb_cppi41_info[0];

	if (!glue->dma_init_done)
		return ;

	numch =  glue->max_dma_channel;
	/* disable the interrupts */
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_eoi, 0);
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_enable_set, 0);
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_usb0_dma_enable, 0);

	order = get_count_order(numch);
	blknum = cppi_info->dma_block;

	/* uninit cppi41 dma & queue mgr */
	cppi41_dma_block_uninit(blknum, cppi_info->q_mgr, order,
			glue->dma_sched_table, numch);
	cppi41_queue_mgr_uninit(cppi_info->q_mgr);

	/* free the irq_resource */
	free_irq(glue->dma_irq, glue);

	/* free cppi resource */
	kfree(cppi_info->ep_dma_ch);
	kfree(glue->dma_sched_table);
	glue->dma_init_done = 0;
}

static irqreturn_t cppi41dma_interrupt(int irq, void *hci)
{
	struct dsps_glue *glue = hci;
	u32 intr_status;
	irqreturn_t ret = IRQ_NONE;
	u32 q_cmpl_status_0, q_cmpl_status_1, q_cmpl_status_2;
	u32 usb0_tx_intr, usb0_rx_intr;
	u32 usb1_tx_intr, usb1_rx_intr;
	void *q_mgr_base = cppi41_queue_mgr[0].q_mgr_rgn_base;
	unsigned long flags;
	struct platform_device *pdev;
	struct device *dev;
	struct musb *musb;

	/*
	 * CPPI 4.1 interrupts share the same IRQ and the EOI register but
	 * don't get reflected in the interrupt source/mask registers.
	 */
	/*
	 * Check for the interrupts from Tx/Rx completion queues; they
	 * are level-triggered and will stay asserted until the queues
	 * are emptied.  We're using the queue pending register 0 as a
	 * substitute for the interrupt status register and reading it
	 * directly for speed.
	 */
	intr_status = dsps_readl(glue->usbss_addr, glue->wrp->usbss.irq_status);

	if (intr_status)
		dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_status,
			intr_status);
	else
		pr_err("spurious usbss intr\n");

	q_cmpl_status_0 = dsps_readl(q_mgr_base, CPPI41_QSTATUS_REG2);
	q_cmpl_status_1 = dsps_readl(q_mgr_base, CPPI41_QSTATUS_REG3);
	q_cmpl_status_2 = dsps_readl(q_mgr_base, CPPI41_QSTATUS_REG4);

	/* USB0 tx/rx completion */
	/* usb0 tx completion interrupt for ep1..15 */
	usb0_tx_intr = (q_cmpl_status_0 >> 29) |
			((q_cmpl_status_1 & 0xFFF) << 3);
	usb0_rx_intr = ((q_cmpl_status_1 & 0x07FFe000) >> 13);

	usb1_tx_intr = (q_cmpl_status_1 >> 29) |
			((q_cmpl_status_2 & 0xFFF) << 3);
	usb1_rx_intr = ((q_cmpl_status_2 & 0x0fffe000) >> 13);

	/* get proper musb handle based usb0/usb1 ctrl-id */
	pdev = glue->musb[0];
	dev = &pdev->dev;
	musb = (struct musb *)dev_get_drvdata(&pdev->dev);

	dev_dbg(musb->controller, "CPPI 4.1 IRQ: Tx %x, Rx %x\n", usb0_tx_intr,
				usb0_rx_intr);
	if (musb && (usb0_tx_intr || usb0_rx_intr)) {
		spin_lock_irqsave(&musb->lock, flags);
		cppi41_completion(musb, usb0_rx_intr,
					usb0_tx_intr);
		spin_unlock_irqrestore(&musb->lock, flags);
		ret = IRQ_HANDLED;
	}

	dev_dbg(musb->controller, "CPPI 4.1 IRQ: Tx %x, Rx %x\n", usb1_tx_intr,
		usb1_rx_intr);
	pdev = glue->musb[1];
	dev = &pdev->dev;
	musb = (struct musb *)dev_get_drvdata(&pdev->dev);

	if (musb && (usb1_rx_intr || usb1_tx_intr)) {
		spin_lock_irqsave(&musb->lock, flags);
		cppi41_completion(musb, usb1_rx_intr,
			usb1_tx_intr);
		spin_unlock_irqrestore(&musb->lock, flags);
		ret = IRQ_HANDLED;
	}

	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_eoi, 0);

	return ret;
}
#endif /* CONFIG_USB_TI_CPPI41_DMA */

/**
 * musb_dsps_phy_control - phy on/off
 * @glue: struct dsps_glue *
 * @id: musb instance
 * @on: flag for phy to be switched on or off
 *
 * This is to enable the PHY using usb_ctrl register in system control
 * module space.
 *
 * XXX: This function will be removed once we have a seperate driver for
 * control module
 */
static void musb_dsps_phy_control(struct dsps_glue *glue, u8 id, u8 on)
{
	u32 usbphycfg;

	usbphycfg = readl(glue->usb_ctrl[id]);

	if (on) {
		usbphycfg &= ~(USBPHY_CM_PWRDN | USBPHY_OTG_PWRDN);
		usbphycfg |= USBPHY_OTGVDET_EN | USBPHY_OTGSESSEND_EN;
	} else {
		usbphycfg |= USBPHY_CM_PWRDN | USBPHY_OTG_PWRDN;
	}

	writel(usbphycfg, glue->usb_ctrl[id]);
}
/**
 * dsps_musb_enable - enable interrupts
 */
static void dsps_musb_enable(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev->parent);
	struct dsps_glue *glue = platform_get_drvdata(pdev);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	void __iomem *reg_base = musb->ctrl_base;
	u32 epmask, coremask;

	/* Workaround: setup IRQs through both register sets. */
	epmask = ((musb->epmask & wrp->txep_mask) << wrp->txep_shift) |
	       ((musb->epmask & wrp->rxep_mask) << wrp->rxep_shift);
	coremask = (wrp->usb_bitmap & ~MUSB_INTR_SOF);

	dsps_writel(reg_base, wrp->epintr_set, epmask);
	dsps_writel(reg_base, wrp->coreintr_set, coremask);
	/* Force the DRVVBUS IRQ so we can start polling for ID change. */
	dsps_writel(reg_base, wrp->coreintr_set,
		    (1 << wrp->drvvbus) << wrp->usb_shift);
}

/**
 * dsps_musb_disable - disable HDRC and flush interrupts
 */
static void dsps_musb_disable(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev->parent);
	struct dsps_glue *glue = platform_get_drvdata(pdev);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	void __iomem *reg_base = musb->ctrl_base;

	dsps_writel(reg_base, wrp->coreintr_clear, wrp->usb_bitmap);
	dsps_writel(reg_base, wrp->epintr_clear,
			 wrp->txep_bitmap | wrp->rxep_bitmap);
	dsps_writeb(musb->mregs, MUSB_DEVCTL, 0);
	dsps_writel(reg_base, wrp->eoi, 0);
}

static void otg_timer(unsigned long _musb)
{
	struct musb *musb = (void *)_musb;
	void __iomem *mregs = musb->mregs;
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev);
	struct dsps_glue *glue = dev_get_drvdata(dev->parent);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	u8 devctl;
	unsigned long flags;

	/*
	 * We poll because DSPS IP's won't expose several OTG-critical
	 * status change events (from the transceiver) otherwise.
	 */
	devctl = dsps_readb(mregs, MUSB_DEVCTL);
	dev_dbg(musb->controller, "Poll devctl %02x (%s)\n", devctl,
				otg_state_string(musb->xceiv->state));

	spin_lock_irqsave(&musb->lock, flags);
	switch (musb->xceiv->state) {
	case OTG_STATE_A_WAIT_BCON:
		devctl &= ~MUSB_DEVCTL_SESSION;
		dsps_writeb(musb->mregs, MUSB_DEVCTL, devctl);

		devctl = dsps_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl & MUSB_DEVCTL_BDEVICE) {
			musb->xceiv->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(musb);
		} else {
			musb->xceiv->state = OTG_STATE_A_IDLE;
			MUSB_HST_MODE(musb);
		}
		break;
	case OTG_STATE_A_WAIT_VFALL:
		musb->xceiv->state = OTG_STATE_A_WAIT_VRISE;
		dsps_writel(musb->ctrl_base, wrp->coreintr_set,
			    MUSB_INTR_VBUSERROR << wrp->usb_shift);
		break;
	case OTG_STATE_B_IDLE:
		devctl = dsps_readb(mregs, MUSB_DEVCTL);
		if (devctl & MUSB_DEVCTL_BDEVICE)
			mod_timer(&glue->timer[pdev->id],
					jiffies + wrp->poll_seconds * HZ);
		else
			musb->xceiv->state = OTG_STATE_A_IDLE;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&musb->lock, flags);
}

static void dsps_musb_try_idle(struct musb *musb, unsigned long timeout)
{
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev);
	struct dsps_glue *glue = dev_get_drvdata(dev->parent);

	if (timeout == 0)
		timeout = jiffies + msecs_to_jiffies(3);

	/* Never idle if active, or when VBUS timeout is not set as host */
	if (musb->is_active || (musb->a_wait_bcon == 0 &&
				musb->xceiv->state == OTG_STATE_A_WAIT_BCON)) {
		dev_dbg(musb->controller, "%s active, deleting timer\n",
				otg_state_string(musb->xceiv->state));
		del_timer(&glue->timer[pdev->id]);
		glue->last_timer[pdev->id] = jiffies;
		return;
	}

	if (time_after(glue->last_timer[pdev->id], timeout) &&
				timer_pending(&glue->timer[pdev->id])) {
		dev_dbg(musb->controller,
			"Longer idle timer already pending, ignoring...\n");
		return;
	}
	glue->last_timer[pdev->id] = timeout;

	dev_dbg(musb->controller, "%s inactive, starting idle timer for %u ms\n",
		otg_state_string(musb->xceiv->state),
			jiffies_to_msecs(timeout - jiffies));
	mod_timer(&glue->timer[pdev->id], timeout);
}

static irqreturn_t dsps_interrupt(int irq, void *hci)
{
	struct musb  *musb = hci;
	void __iomem *reg_base = musb->ctrl_base;
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev);
	struct dsps_glue *glue = dev_get_drvdata(dev->parent);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	u32 epintr, usbintr;

	spin_lock_irqsave(&musb->lock, flags);

	/* Get endpoint interrupts */
	epintr = dsps_readl(reg_base, wrp->epintr_status);
	musb->int_rx = (epintr & wrp->rxep_bitmap) >> wrp->rxep_shift;
	musb->int_tx = (epintr & wrp->txep_bitmap) >> wrp->txep_shift;

	if (epintr)
		dsps_writel(reg_base, wrp->epintr_status, epintr);

	/* Get usb core interrupts */
	usbintr = dsps_readl(reg_base, wrp->coreintr_status);
	if (!usbintr && !epintr)
		goto eoi;

	musb->int_usb =	(usbintr & wrp->usb_bitmap) >> wrp->usb_shift;
	if (usbintr)
		dsps_writel(reg_base, wrp->coreintr_status, usbintr);

	dev_dbg(musb->controller, "usbintr (%x) epintr(%x)\n",
			usbintr, epintr);
	/*
	 * DRVVBUS IRQs are the only proxy we have (a very poor one!) for
	 * DSPS IP's missing ID change IRQ.  We need an ID change IRQ to
	 * switch appropriately between halves of the OTG state machine.
	 * Managing DEVCTL.SESSION per Mentor docs requires that we know its
	 * value but DEVCTL.BDEVICE is invalid without DEVCTL.SESSION set.
	 * Also, DRVVBUS pulses for SRP (but not at 5V) ...
	 */
	if (is_host_active(musb) && usbintr & MUSB_INTR_BABBLE)
		pr_info("CAUTION: musb: Babble Interrupt Occurred\n");

	if (usbintr & ((1 << wrp->drvvbus) << wrp->usb_shift)) {
		int drvvbus = dsps_readl(reg_base, wrp->status);
		void __iomem *mregs = musb->mregs;
		u8 devctl = dsps_readb(mregs, MUSB_DEVCTL);
		int err;

		err = musb->int_usb & MUSB_INTR_VBUSERROR;
		if (err) {
			/*
			 * The Mentor core doesn't debounce VBUS as needed
			 * to cope with device connect current spikes. This
			 * means it's not uncommon for bus-powered devices
			 * to get VBUS errors during enumeration.
			 *
			 * This is a workaround, but newer RTL from Mentor
			 * seems to allow a better one: "re"-starting sessions
			 * without waiting for VBUS to stop registering in
			 * devctl.
			 */
			musb->int_usb &= ~MUSB_INTR_VBUSERROR;
			musb->xceiv->state = OTG_STATE_A_WAIT_VFALL;
			mod_timer(&glue->timer[pdev->id],
					jiffies + wrp->poll_seconds * HZ);
			WARNING("VBUS error workaround (delay coming)\n");
		} else if (drvvbus) {
			MUSB_HST_MODE(musb);
			musb->xceiv->otg->default_a = 1;
			musb->xceiv->state = OTG_STATE_A_WAIT_VRISE;
			del_timer(&glue->timer[pdev->id]);
		} else {
			musb->is_active = 0;
			MUSB_DEV_MODE(musb);
			musb->xceiv->otg->default_a = 0;
			musb->xceiv->state = OTG_STATE_B_IDLE;
		}

		/* NOTE: this must complete power-on within 100 ms. */
		dev_dbg(musb->controller, "VBUS %s (%s)%s, devctl %02x\n",
				drvvbus ? "on" : "off",
				otg_state_string(musb->xceiv->state),
				err ? " ERROR" : "",
				devctl);
		ret = IRQ_HANDLED;
	}

	if (musb->int_tx || musb->int_rx || musb->int_usb)
		ret |= musb_interrupt(musb);

 eoi:
	/* EOI needs to be written for the IRQ to be re-asserted. */
	if (ret == IRQ_HANDLED || epintr || usbintr)
		dsps_writel(reg_base, wrp->eoi, 1);

	/* Poll for ID change */
	if (musb->xceiv->state == OTG_STATE_B_IDLE)
		mod_timer(&glue->timer[pdev->id],
			 jiffies + wrp->poll_seconds * HZ);

	spin_unlock_irqrestore(&musb->lock, flags);

	return ret;
}

static int dsps_musb_init(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev);
	struct dsps_glue *glue = dev_get_drvdata(dev->parent);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	void __iomem *reg_base = musb->ctrl_base;
	u32 rev, val;
	int status;

	/* mentor core register starts at offset of 0x400 from musb base */
	musb->mregs += wrp->musb_core_offset;

	/* NOP driver needs change if supporting dual instance */
	usb_nop_xceiv_register();
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(musb->xceiv))
		return -EPROBE_DEFER;

	/* Returns zero if e.g. not clocked */
	rev = dsps_readl(reg_base, wrp->revision);
	if (!rev) {
		status = -ENODEV;
		goto err0;
	}

	setup_timer(&glue->timer[pdev->id], otg_timer, (unsigned long) musb);

	/* Reset the musb */
	dsps_writel(reg_base, wrp->control, (1 << wrp->reset));

	/* Start the on-chip PHY and its PLL. */
	musb_dsps_phy_control(glue, pdev->id, 1);

	musb->isr = dsps_interrupt;

	/* reset the otgdisable bit, needed for host mode to work */
	val = dsps_readl(reg_base, wrp->phy_utmi);
	val &= ~(1 << wrp->otg_disable);
	dsps_writel(musb->ctrl_base, wrp->phy_utmi, val);

	/* clear level interrupt */
	dsps_writel(reg_base, wrp->eoi, 0);

	return 0;
err0:
	usb_put_phy(musb->xceiv);
	usb_nop_xceiv_unregister();
	return status;
}

static int dsps_musb_exit(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct platform_device *pdev = to_platform_device(dev);
	struct dsps_glue *glue = dev_get_drvdata(dev->parent);

	del_timer_sync(&glue->timer[pdev->id]);

	/* Shutdown the on-chip PHY and its PLL. */
	musb_dsps_phy_control(glue, pdev->id, 0);

	/* NOP driver needs change if supporting dual instance */
	usb_put_phy(musb->xceiv);
	usb_nop_xceiv_unregister();

	return 0;
}

static struct musb_platform_ops dsps_ops = {
	.init		= dsps_musb_init,
	.exit		= dsps_musb_exit,

	.enable		= dsps_musb_enable,
	.disable	= dsps_musb_disable,

	.try_idle	= dsps_musb_try_idle,
};

static u64 musb_dmamask = DMA_BIT_MASK(32);

static int dsps_create_musb_pdev(struct dsps_glue *glue, u8 id)
{
	struct device *dev = glue->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct musb_hdrc_platform_data  *pdata = dev->platform_data;
	struct device_node *np = pdev->dev.of_node;
	struct musb_hdrc_config	*config;
	struct platform_device	*musb;
	struct resource *res;
	struct resource	resources[2];
	char res_name[11];
	int ret;

	resources[0].start = dsps_control_module_phys[id];
	resources[0].end = resources[0].start + SZ_4 - 1;
	resources[0].flags = IORESOURCE_MEM;

	glue->usb_ctrl[id] = devm_request_and_ioremap(&pdev->dev, resources);
	if (glue->usb_ctrl[id] == NULL) {
		dev_err(dev, "Failed to obtain usb_ctrl%d memory\n", id);
		ret = -ENODEV;
		goto err0;
	}

	/* first resource is for usbss, so start index from 1 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, id + 1);
	if (!res) {
		dev_err(dev, "failed to get memory for instance %d\n", id);
		ret = -ENODEV;
		goto err0;
	}
	res->parent = NULL;
	resources[0] = *res;

	/* first resource is for usbss, so start index from 1 */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, id + 1);
	if (!res) {
		dev_err(dev, "failed to get irq for instance %d\n", id);
		ret = -ENODEV;
		goto err0;
	}
	res->parent = NULL;
	resources[1] = *res;
	resources[1].name = "mc";

	/* allocate the child platform device */
	musb = platform_device_alloc("musb-hdrc", PLATFORM_DEVID_AUTO);
	if (!musb) {
		dev_err(dev, "failed to allocate musb device\n");
		ret = -ENOMEM;
		goto err0;
	}

	musb->dev.parent		= dev;
	musb->dev.dma_mask		= &musb_dmamask;
	musb->dev.coherent_dma_mask	= musb_dmamask;

	glue->musb[id]			= musb;

	ret = platform_device_add_resources(musb, resources, 2);
	if (ret) {
		dev_err(dev, "failed to add resources\n");
		goto err2;
	}

	if (np) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev,
				"failed to allocate musb platfrom data\n");
			ret = -ENOMEM;
			goto err2;
		}

		config = devm_kzalloc(&pdev->dev, sizeof(*config), GFP_KERNEL);
		if (!config) {
			dev_err(&pdev->dev,
				"failed to allocate musb hdrc config\n");
			goto err2;
		}

		of_property_read_u32(np, "num-eps", (u32 *)&config->num_eps);
		of_property_read_u32(np, "ram-bits", (u32 *)&config->ram_bits);
		snprintf(res_name, sizeof(res_name), "port%d-mode", id);
		of_property_read_u32(np, res_name, (u32 *)&pdata->mode);
		of_property_read_u32(np, "power", (u32 *)&pdata->power);
		config->multipoint = of_property_read_bool(np, "multipoint");

		pdata->config		= config;
	}

	pdata->platform_ops		= &dsps_ops;

	ret = platform_device_add_data(musb, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(dev, "failed to add platform_data\n");
		goto err2;
	}

	ret = platform_device_add(musb);
	if (ret) {
		dev_err(dev, "failed to register musb device\n");
		goto err2;
	}

	return 0;

err2:
	platform_device_put(musb);
err0:
	return ret;
}

static int dsps_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	const struct dsps_musb_wrapper *wrp;
	struct dsps_glue *glue;
	struct resource *iomem;
	int ret, i;

	match = of_match_node(musb_dsps_of_match, np);
	if (!match) {
		dev_err(&pdev->dev, "fail to get matching of_match struct\n");
		ret = -EINVAL;
		goto err0;
	}
	wrp = match->data;

	/* allocate glue */
	glue = kzalloc(sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "unable to allocate glue memory\n");
		ret = -ENOMEM;
		goto err0;
	}

	/* get memory resource */
	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iomem) {
		dev_err(&pdev->dev, "failed to get usbss mem resourse\n");
		ret = -ENODEV;
		goto err1;
	}

	glue->dev = &pdev->dev;
	iomem[0].flags = IORESOURCE_MEM;
	glue->usbss_addr = devm_request_and_ioremap(&pdev->dev, iomem);
	if (glue->usbss_addr == NULL) {
		dev_err(&pdev->dev, "Failed to obtain usbss_addr memory\n");
		ret = -ENODEV;
		goto err1;
	}

	glue->wrp = kmemdup(wrp, sizeof(*wrp), GFP_KERNEL);
	if (!glue->wrp) {
		dev_err(&pdev->dev, "failed to duplicate wrapper struct memory\n");
		ret = -ENOMEM;
		goto err1;
	}
	platform_set_drvdata(pdev, glue);
	glue->first = 1;

	/* enable the usbss clocks */
	pm_runtime_enable(&pdev->dev);

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm_runtime_get_sync FAILED");
		goto err2;
	}

#ifdef CONFIG_USB_TI_CPPI41_DMA
	/* get memory resource */
	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!iomem) {
		dev_err(&pdev->dev, "failed to get usbss mem resourse\n");
		ret = -ENODEV;
		goto err1;
	}

	iomem[0].flags = IORESOURCE_MEM;
	glue->dma_addr = devm_request_and_ioremap(&pdev->dev, iomem);
	if (glue->dma_addr == NULL) {
		dev_err(&pdev->dev, "Failed to obtain usbss_addr memory\n");
		ret = -ENODEV;
		goto err1;
	}

	/* first resource is for usbss, so start index from 1 */
	iomem = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!iomem) {
		dev_err(&pdev->dev, "failed to get irq for instance %d\n", 0);
		ret = -ENODEV;
		goto err0;
	}
	glue->dma_irq = iomem->start;

	/* clear any USBSS interrupts */
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_eoi, 0);
	dsps_writel(glue->usbss_addr, glue->wrp->usbss.irq_status,
		dsps_readl(glue->usbss_addr, glue->wrp->usbss.irq_status));

	/* initialize the cppi41dma init */
	ret = cppi41_init(glue);
	if (ret) {
		dev_err(&pdev->dev, "cppi4.1 dma init failed\n");
		return ret;
	}
#endif

	/* create the child platform device for all instances of musb */
	for (i = 0; i < wrp->instances ; i++) {
		ret = dsps_create_musb_pdev(glue, i);
		if (ret != 0) {
			dev_err(&pdev->dev, "failed to create child pdev\n");
			/* release resources of previously created instances */
			for (i--; i >= 0 ; i--)
				platform_device_unregister(glue->musb[i]);
			goto err3;
		}
	}


	return 0;

err3:
	pm_runtime_put(&pdev->dev);
err2:
	pm_runtime_disable(&pdev->dev);
	kfree(glue->wrp);
err1:
	kfree(glue);
err0:
	return ret;
}
static int dsps_remove(struct platform_device *pdev)
{
	struct dsps_glue *glue = platform_get_drvdata(pdev);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	int i;

	/* delete the child platform device */
	for (i = 0; i < wrp->instances ; i++)
		platform_device_unregister(glue->musb[i]);

	/* disable usbss clocks */
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

#ifdef CONFIG_USB_TI_CPPI41_DMA
	cppi41_free(glue);
#endif
	kfree(glue->wrp);
	kfree(glue);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void dsps_save_context(struct dsps_glue *glue)
{
	struct dsps_usbss_regs *usbss = &glue->usbss_regs;
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	u8 i, j;

	/* save USBSS register */
	usbss->irq_en_set = dsps_readl(glue->usbss_addr,
				wrp->epintr_set);

#ifdef CONFIG_USB_TI_CPPI41_DMA
	for (i = 0 ; i < 4 ; i++) {
		usbss->irq_dma_th_tx0[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_tx0_dma_th_enb + (4 * i));
		usbss->irq_dma_th_rx0[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_rx0_dma_th_enb + (4 * i));
		usbss->irq_dma_th_tx1[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_tx1_dma_th_enb + (4 * i));
		usbss->irq_dma_th_rx1[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_rx1_dma_th_enb + (4 * i));

		usbss->irq_frame_th_tx0[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_tx0_frame_th_enb + (4 * i));
		usbss->irq_frame_th_rx0[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_rx0_frame_th_enb + (4 * i));
		usbss->irq_frame_th_tx1[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_tx1_frame_th_enb + (4 * i));
		usbss->irq_frame_th_rx1[i] = dsps_readl(glue->usbss_addr,
			wrp->usbss.irq_rx1_frame_th_enb + (4 * i));
	}
	for (i = 0 ; i < 2 ; i++) {
		usbss->irq_dma_en[i] = dsps_readl(glue->usbss_addr,
				wrp->usbss.irq_usb0_dma_enable + (4 * i));
		usbss->irq_frame_en[i] = dsps_readl(glue->usbss_addr,
				wrp->usbss.irq_usb0_frame_enable + (4 * i));
	}
#endif
	/* save usbX register */
	for (i = 0 ; i < wrp->instances ; i++) {
		struct dsps_usb_regs *usb = &glue->usb_regs[i];
		const struct dsps_musb_wrapper *wrp = glue->wrp;
		struct musb *musb = platform_get_drvdata(glue->musb[i]);
		void __iomem *cbase = musb->ctrl_base;

		musb_save_context(musb);
		usb->control = musb_readl(cbase, wrp->control);

		for (j = 0 ; j < 2 ; j++)
			usb->irq_en_set[j] = musb_readl(cbase,
						wrp->epintr_set + (4 * j));
#ifdef CONFIG_USB_TI_CPPI41_DMA
		usb->tx_mode = musb_readl(cbase, wrp->tx_mode);
		usb->rx_mode = musb_readl(cbase, wrp->rx_mode);

		for (j = 0 ; j < 15 ; j++)
			usb->grndis_size[j] = musb_readl(cbase,
						wrp->g_rndis + (j << 2));

		usb->auto_req = musb_readl(cbase, wrp->auto_req);
		usb->teardn = musb_readl(cbase, wrp->tear_down);
		usb->th_xdma_idle = musb_readl(cbase, wrp->th_xdma_idle);
#endif
		usb->srp_fix = musb_readl(cbase, wrp->srp_fix_time);
		usb->phy_utmi = musb_readl(cbase, wrp->phy_utmi);
		usb->mgc_utmi_loopback = musb_readl(cbase,
						wrp->mgc_utmi_lpback);
		usb->mode = musb_readl(cbase, wrp->mode);
	}
	/* save CPPI4.1 DMA register */
#ifdef CONFIG_USB_TI_CPPI41_DMA
	/* save CPPI4.1 DMA register for dma block 0 */
	cppi41_save_context(0);
#endif
}

static void dsps_restore_context(struct dsps_glue *glue)
{
	struct dsps_usbss_regs *usbss = &glue->usbss_regs;
	const struct dsps_musb_wrapper *wrp = glue->wrp;
#ifdef CONFIG_USB_TI_CPPI41_DMA
	void __iomem *usbss_addr = glue->usbss_addr;
	struct dsps_usbss_wrapper *usbsswrp = &glue->wrp->usbss;
#endif
	u8 i, j;

	/* restore USBSS register */
	dsps_writel(glue->usbss_addr, wrp->epintr_set,
			usbss->irq_en_set);

#ifdef CONFIG_USB_TI_CPPI41_DMA
	for (i = 0 ; i < 4 ; i++) {

		dsps_writel(usbss_addr, usbsswrp->irq_tx0_dma_th_enb + (4 * i),
				usbss->irq_dma_th_tx0[i]);
		dsps_writel(usbss_addr, usbsswrp->irq_rx0_dma_th_enb + (4 * i),
				usbss->irq_dma_th_rx0[i]);
		dsps_writel(usbss_addr, usbsswrp->irq_tx1_dma_th_enb + (4 * i),
				usbss->irq_dma_th_tx1[i]);
		dsps_writel(usbss_addr, usbsswrp->irq_rx1_dma_th_enb + (4 * i),
				usbss->irq_dma_th_rx1[i]);

		dsps_writel(usbss_addr, usbsswrp->irq_tx0_frame_th_enb + (4 * i)
					, usbss->irq_frame_th_tx0[i]);
		dsps_writel(usbss_addr, usbsswrp->irq_rx0_frame_th_enb + (4 * i)
					, usbss->irq_frame_th_rx0[i]);
		dsps_writel(usbss_addr, usbsswrp->irq_tx1_frame_th_enb + (4 * i)
					, usbss->irq_frame_th_tx1[i]);
		dsps_writel(usbss_addr, usbsswrp->irq_rx1_frame_th_enb + (4 * i)
					, usbss->irq_frame_th_rx1[i]);
	}
	for (i = 0 ; i < 2 ; i++) {
		dsps_writel(usbss_addr, usbsswrp->irq_usb0_dma_enable + (4 * i),
				usbss->irq_dma_en[i]);
		dsps_writel(usbss_addr,
			usbsswrp->irq_usb0_frame_enable + (4 * i),
			usbss->irq_frame_en[i]);
	}
#endif
	/* restore usbX register */
	for (i = 0 ; i < 2 ; i++) {
		struct dsps_usb_regs *usb = &glue->usb_regs[i];
		const struct dsps_musb_wrapper *wrp = glue->wrp;
		struct musb *musb = platform_get_drvdata(glue->musb[i]);
		void __iomem *cbase = musb->ctrl_base;

		musb_restore_context(musb);
		musb_writel(cbase, wrp->control, usb->control);

		for (j = 0 ; j < 2 ; j++)
			musb_writel(cbase, wrp->epintr_set + (4 * j),
					usb->irq_en_set[j]);

#ifdef CONFIG_USB_TI_CPPI41_DMA
		musb_writel(cbase, wrp->tx_mode, usb->tx_mode);
		musb_writel(cbase, wrp->rx_mode, usb->rx_mode);

		for (j = 0 ; j < 15 ; j++)
			musb_writel(cbase, wrp->g_rndis + (j << 2),
					usb->grndis_size[j]);

		musb_writel(cbase, wrp->auto_req, usb->auto_req);
		musb_writel(cbase, wrp->tear_down, usb->teardn);
		musb_writel(cbase, wrp->th_xdma_idle, usb->th_xdma_idle);
#endif
		musb_writel(cbase, wrp->srp_fix_time, usb->srp_fix);
		musb_writel(cbase, wrp->phy_utmi, usb->phy_utmi);
		musb_writel(cbase, wrp->mgc_utmi_lpback,
				usb->mgc_utmi_loopback);
		musb_writel(cbase, wrp->mode, usb->mode);
	}
	/* restore CPPI4.1 DMA register */
#ifdef CONFIG_USB_TI_CPPI41_DMA
	/* save CPPI4.1 DMA register for dma block 0 */
	cppi41_restore_context(0);

	/* controller needs 200ms delay to resume */
	msleep(200);
#endif
}

static int dsps_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev->parent);
	struct dsps_glue *glue = platform_get_drvdata(pdev);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	int i;

	/* save wrappers and cppi4.1 dma register */
	dsps_save_context(glue);

	for (i = 0; i < wrp->instances; i++)
		musb_dsps_phy_control(glue, i, 0);

	return 0;
}

static int dsps_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev->parent);
	struct dsps_glue *glue = platform_get_drvdata(pdev);
	const struct dsps_musb_wrapper *wrp = glue->wrp;
	int i;

	/*
	 * ignore first call of resume as all registers are not yet
	 * initialized
	 */
	if (glue->first) {
		glue->first = 0;
		return 0;
	}

	for (i = 0; i < wrp->instances; i++)
		musb_dsps_phy_control(glue, i, 1);

	/* restore wrappers and cppi4.1 dma register */
	dsps_restore_context(glue);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(dsps_pm_ops, dsps_suspend, dsps_resume);

static const struct dsps_musb_wrapper ti81xx_driver_data = {
	.revision		= 0x00,
	.control		= 0x14,
	.status			= 0x18,
	.eoi			= 0x24,
	.epintr_set		= 0x38,
	.epintr_clear		= 0x40,
	.epintr_status		= 0x30,
	.coreintr_set		= 0x3c,
	.coreintr_clear		= 0x44,
	.coreintr_status	= 0x34,
#ifdef CONFIG_USB_TI_CPPI41_DMA
	.tx_mode		= 0x70,
	.rx_mode		= 0x74,
	.g_rndis		= 0x80,
	.auto_req		= 0xd0,
	.tear_down		= 0xd8,
	.th_xdma_idle		= 0xdc,
#endif
	.srp_fix_time		= 0xd4,
	.phy_utmi		= 0xe0,
	.mgc_utmi_lpback	= 0xe4,
	.mode			= 0xe8,
	.reset			= 0,
	.otg_disable		= 21,
	.iddig			= 8,
	.usb_shift		= 0,
	.usb_mask		= 0x1ff,
	.usb_bitmap		= (0x1ff << 0),
	.drvvbus		= 8,
	.txep_shift		= 0,
	.txep_mask		= 0xffff,
	.txep_bitmap		= (0xffff << 0),
	.rxep_shift		= 16,
	.rxep_mask		= 0xfffe,
	.rxep_bitmap		= (0xfffe << 16),
	.musb_core_offset	= 0x400,
	.poll_seconds		= 2,
	.instances		= 2,
	.usbss			=  {
		.revision	= 0x00,
		.syscfg		= 0x10,
		.irq_eoi	= 0x20,
		.irq_status_raw = 0x24,
		.irq_status	= 0x28,
		.irq_enable_set = 0x2c,
		.irq_enable_clr = 0x30,
		.irq_tx0_dma_th_enb = 0x100,
		.irq_tx0_dma_th_enb = 0x110,
		.irq_tx1_dma_th_enb = 0x120,
		.irq_tx1_dma_th_enb = 0x130,
		.irq_usb0_dma_enable = 0x140,
		.irq_usb1_dma_enable = 0x144,
		.irq_tx0_frame_th_enb = 0x200,
		.irq_tx0_frame_th_enb = 0x210,
		.irq_tx1_frame_th_enb = 0x220,
		.irq_tx1_frame_th_enb = 0x230,
		.irq_usb0_frame_enable = 0x240,
		.irq_usb1_frame_enable = 0x244,
	}
};

static const struct platform_device_id musb_dsps_id_table[] = {
	{
		.name	= "musb-ti81xx",
		.driver_data	= (kernel_ulong_t) &ti81xx_driver_data,
	},
	{  },	/* Terminating Entry */
};
MODULE_DEVICE_TABLE(platform, musb_dsps_id_table);

#ifdef CONFIG_OF
static const struct of_device_id musb_dsps_of_match[] = {
	{ .compatible = "ti,musb-am33xx",
		.data = (void *) &ti81xx_driver_data, },
	{  },
};
MODULE_DEVICE_TABLE(of, musb_dsps_of_match);
#endif

static struct platform_driver dsps_usbss_driver = {
	.probe		= dsps_probe,
	.remove         = dsps_remove,
	.driver         = {
		.name   = "musb-dsps",
		.pm	= &dsps_pm_ops,
		.of_match_table	= of_match_ptr(musb_dsps_of_match),
	},
	.id_table	= musb_dsps_id_table,
};

MODULE_DESCRIPTION("TI DSPS MUSB Glue Layer");
MODULE_AUTHOR("Ravi B <ravibabu@ti.com>");
MODULE_AUTHOR("Ajay Kumar Gupta <ajay.gupta@ti.com>");
MODULE_LICENSE("GPL v2");

static int __init dsps_init(void)
{
	return platform_driver_register(&dsps_usbss_driver);
}
subsys_initcall(dsps_init);

static void __exit dsps_exit(void)
{
	platform_driver_unregister(&dsps_usbss_driver);
}
module_exit(dsps_exit);
