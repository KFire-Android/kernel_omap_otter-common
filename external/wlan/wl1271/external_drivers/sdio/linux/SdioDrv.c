/*
 * SdioDrv.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Texas Instruments nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl4030.h>
#include <linux/errno.h>
#include <linux/clk.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 31))
#include <plat/hardware.h>
#include <plat/board.h>
#include <plat/clock.h>
#include <plat/dma.h>
#include <plat/io.h>
#include <plat/resource.h>
#define IO_ADDRESS(pa)	OMAP2_L4_IO_ADDRESS(pa)
#else
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/clock.h>
#include <mach/dma.h>
#include <mach/io.h>
#include <mach/resource.h>
#endif
typedef void *TI_HANDLE;
#include "host_platform.h"
#include "SdioDrvDbg.h"
#include "SdioDrv.h"

/* #define TI_SDIO_DEBUG */

#ifndef CONFIG_MMC_EMBEDDED_SDIO

#define SDIOWQ_NAME			"sdio_wq"

/*
 * HSMMC Address and DMA Settings
 */
static unsigned long TIWLAN_MMC_CONTROLLER = 2; /* MMC3 */
static unsigned long TIWLAN_MMC_CONTROLLER_BASE_ADDR = OMAP_HSMMC3_BASE;
#define TIWLAN_MMC_CONTROLLER_BASE_SIZE	512
#define TIWLAN_MMC_MAX_DMA		8192
static unsigned long TIWLAN_MMC_DMA_TX = OMAP34XX_DMA_MMC3_TX;
static unsigned long TIWLAN_MMC_DMA_RX = OMAP34XX_DMA_MMC3_RX;
static unsigned long OMAP_MMC_IRQ = INT_MMC3_IRQ;

#define OMAP_MMC_MASTER_CLOCK          96000000
/*
 *  HSMMC Host Controller Registers
 */
#define OMAP_HSMMC_SYSCONFIG           0x0010
#define OMAP_HSMMC_SYSSTATUS           0x0014
#define OMAP_HSMMC_CSRE                0x0024
#define OMAP_HSMMC_SYSTEST             0x0028
#define OMAP_HSMMC_CON                 0x002C
#define OMAP_HSMMC_BLK                 0x0104
#define OMAP_HSMMC_ARG                 0x0108
#define OMAP_HSMMC_CMD                 0x010C
#define OMAP_HSMMC_RSP10               0x0110
#define OMAP_HSMMC_RSP32               0x0114
#define OMAP_HSMMC_RSP54               0x0118
#define OMAP_HSMMC_RSP76               0x011C
#define OMAP_HSMMC_DATA                0x0120
#define OMAP_HSMMC_PSTATE              0x0124
#define OMAP_HSMMC_HCTL                0x0128
#define OMAP_HSMMC_SYSCTL              0x012C
#define OMAP_HSMMC_STAT                0x0130
#define OMAP_HSMMC_IE                  0x0134
#define OMAP_HSMMC_ISE                 0x0138
#define OMAP_HSMMC_AC12                0x013C
#define OMAP_HSMMC_CAPA                0x0140
#define OMAP_HSMMC_CUR_CAPA            0x0148
#define OMAP_HSMMC_REV                 0x01FC

#define VS18                           (1 << 26)
#define VS30                           (1 << 25)
#define SRA                            (1 << 24)
#define SDVS18                         (0x5 << 9)
#define SDVS30                         (0x6 << 9)
#define SDVSCLR                        0xFFFFF1FF
#define SDVSDET                        0x00000400
#define SIDLE_MODE                     (0x2 << 3)
#define AUTOIDLE                       0x1
#define SDBP                           (1 << 8)
#define DTO                            0xE
#define ICE                            0x1
#define ICS                            0x2
#define CEN                            (1 << 2)
#define CLKD_MASK                      0x0000FFC0
#define IE_EN_MASK                     0x317F0137
#define INIT_STREAM                    (1 << 1)
#define DP_SELECT                      (1 << 21)
#define DDIR                           (1 << 4)
#define DMA_EN                         0x1
#define MSBS                           (1 << 5)
#define BCE                            (1 << 1)
#define ONE_BIT                        (~(0x2))
#define EIGHT_BIT                      (~(0x20))
#define CC                             0x1
#define TC                             0x02
#define OD                             0x1
#define BRW                            0x400
#define BRR                            0x800
#define BRE                            (1 << 11)
#define BWE                            (1 << 10)
#define SBGR                           (1 << 16)
#define CT                             (1 << 17)
#define SDIO_READ                      (1 << 31)
#define SDIO_BLKMODE                   (1 << 27)
#define OMAP_HSMMC_ERR                 (1 << 15)  /* Any error */
#define OMAP_HSMMC_CMD_TIMEOUT         (1 << 16)  /* Com mand response time-out */
#define OMAP_HSMMC_DATA_TIMEOUT        (1 << 20)  /* Data response time-out */
#define OMAP_HSMMC_CMD_CRC             (1 << 17)  /* Command CRC error */
#define OMAP_HSMMC_DATA_CRC            (1 << 21)  /* Date CRC error */
#define OMAP_HSMMC_CARD_ERR            (1 << 28)  /* Card ERR */
#define OMAP_HSMMC_STAT_CLEAR          0xFFFFFFFF
#define INIT_STREAM_CMD                0x00000000
#define INT_CLEAR                      0x00000000
#define BLK_CLEAR                      0x00000000

/* SCM CONTROL_DEVCONF1 MMC1 overwrite but */

#define MMC1_ACTIVE_OVERWRITE          (1 << 31)

#define sdio_blkmode_regaddr           0x2000
#define sdio_blkmode_mask              0xFF00

#define IO_RW_DIRECT_MASK              0xF000FF00
#define IO_RW_DIRECT_ARG_MASK          0x80001A00

#define RMASK                          (MMC_RSP_MASK | MMC_RSP_CRC)
#define MMC_TIMEOUT_MS                 100 /*on the new 2430 it was 20, i changed back to 100*//* obc */
#define MMCA_VSN_4                     4

#define VMMC1_DEV_GRP                  0x27
#define P1_DEV_GRP                     0x20
#define VMMC1_DEDICATED                0x2A
#define VSEL_3V                        0x02
#define VSEL_18V                       0x00
#define PBIAS_3V                       0x03
#define PBIAS_18V                      0x02
#define PBIAS_LITE                     0x04A0
#define PBIAS_CLR                      0x00

#define OMAP_MMC_REGS_BASE             IO_ADDRESS(TIWLAN_MMC_CONTROLLER_BASE_ADDR)

/*
 * MMC Host controller read/write API's.
 */
#define OMAP_HSMMC_READ_OFFSET(offset) (__raw_readl((OMAP_MMC_REGS_BASE) + (offset)))
#define OMAP_HSMMC_READ(reg)           (__raw_readl((OMAP_MMC_REGS_BASE) + OMAP_HSMMC_##reg))
#define OMAP_HSMMC_WRITE(reg, val)     (__raw_writel((val), (OMAP_MMC_REGS_BASE) + OMAP_HSMMC_##reg))

#define OMAP_HSMMC_SEND_COMMAND(cmd, arg) do \
{ \
	OMAP_HSMMC_WRITE(ARG, arg); \
	OMAP_HSMMC_WRITE(CMD, cmd); \
} while (0)

#define OMAP_HSMMC_CMD52_WRITE     ((SD_IO_RW_DIRECT    << 24) | (OMAP_HSMMC_CMD_SHORT_RESPONSE << 16))
#define OMAP_HSMMC_CMD52_READ      (((SD_IO_RW_DIRECT   << 24) | (OMAP_HSMMC_CMD_SHORT_RESPONSE << 16)) | DDIR)
#define OMAP_HSMMC_CMD53_WRITE     (((SD_IO_RW_EXTENDED << 24) | (OMAP_HSMMC_CMD_SHORT_RESPONSE << 16)) | DP_SELECT)
#define OMAP_HSMMC_CMD53_READ      (((SD_IO_RW_EXTENDED << 24) | (OMAP_HSMMC_CMD_SHORT_RESPONSE << 16)) | DP_SELECT | DDIR)
#define OMAP_HSMMC_CMD53_READ_DMA  (OMAP_HSMMC_CMD53_READ  | DMA_EN)
#define OMAP_HSMMC_CMD53_WRITE_DMA (OMAP_HSMMC_CMD53_WRITE | DMA_EN)

/* Macros to build commands 52 and 53 in format according to SDIO spec */
#define SDIO_CMD52_READ(v1,v2,v3,v4)        (SDIO_RWFLAG(v1)|SDIO_FUNCN(v2)|SDIO_RAWFLAG(v3)| SDIO_ADDRREG(v4))
#define SDIO_CMD52_WRITE(v1,v2,v3,v4,v5)    (SDIO_RWFLAG(v1)|SDIO_FUNCN(v2)|SDIO_RAWFLAG(v3)| SDIO_ADDRREG(v4)|(v5))
#define SDIO_CMD53_READ(v1,v2,v3,v4,v5,v6)  (SDIO_RWFLAG(v1)|SDIO_FUNCN(v2)|SDIO_BLKM(v3)| SDIO_OPCODE(v4)|SDIO_ADDRREG(v5)|(v6&0x1ff))
#define SDIO_CMD53_WRITE(v1,v2,v3,v4,v5,v6) (SDIO_RWFLAG(v1)|SDIO_FUNCN(v2)|SDIO_BLKM(v3)| SDIO_OPCODE(v4)|SDIO_ADDRREG(v5)|(v6&0x1ff))

#define SDIODRV_MAX_LOOPS	50000

#define VMMC2_DEV_GRP		0x2B
#define VMMC2_DEDICATED		0x2E
#define VSEL_S2_18V		0x05
#define LDO_CLR			0x00
#define VSEL_S2_CLR		0x40
#define GPIO_0_BIT_POS		1 << 0
#define GPIO_1_BIT_POS		1 << 1
#define VSIM_DEV_GRP		0x37
#define VSIM_DEDICATED		0x3A
#define TWL4030_MODULE_PM_RECIEVER	0x13

typedef struct OMAP3430_sdiodrv
{
	struct clk    *fclk, *iclk, *dbclk;
	int           ifclks_enabled;
	spinlock_t    clk_lock;	
	int           dma_tx_channel;
	int           dma_rx_channel;
	int           irq;
	void          (*BusTxnCB)(void* BusTxnHandle, int status);
	void*         BusTxnHandle;
	unsigned int  uBlkSize;
	unsigned int  uBlkSizeShift;
	int           async_status;
	int (*wlanDrvIf_pm_resume)(void);
	int (*wlanDrvIf_pm_suspend)(void);
	struct device *dev;
	dma_addr_t dma_read_addr;
	size_t dma_read_size;
	dma_addr_t dma_write_addr;
	size_t dma_write_size;
	struct workqueue_struct *sdio_wq; /* Work Queue */
	struct work_struct sdiodrv_work;
	struct timer_list inact_timer;
	int inact_timer_running;
} OMAP3430_sdiodrv_t;

struct omap_hsmmc_regs {
        u32 hctl;
        u32 capa;
        u32 sysconfig;
        u32 ise;
        u32 ie;
        u32 con;
        u32 sysctl;
};
static struct omap_hsmmc_regs hsmmc_ctx;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 31))
static struct platform_device dummy_pdev = {
	.dev = {
		.bus = &platform_bus_type,
	},
};
#endif

#define SDIO_DRIVER_NAME 			"TIWLAN_SDIO"

module_param(g_sdio_debug_level, int, 0644);
MODULE_PARM_DESC(g_sdio_debug_level, "debug level");
int g_sdio_debug_level = SDIO_DEBUGLEVEL_ERR;
EXPORT_SYMBOL(g_sdio_debug_level);

OMAP3430_sdiodrv_t g_drv;

static int sdiodrv_irq_requested = 0;
static int sdiodrv_iclk_got = 0;
static int sdiodrv_fclk_got = 0;

static void sdioDrv_hsmmc_save_ctx(void);
static void sdioDrv_hsmmc_restore_ctx(void);
static void sdiodrv_dma_shutdown(void);
static void sdioDrv_inact_timer(unsigned long);

#ifndef TI_SDIO_STANDALONE
void sdio_init( int sdcnum )
{
	if( sdcnum <= 0 )
		return;
	TIWLAN_MMC_CONTROLLER = sdcnum - 1;
	if( sdcnum == 2 ) {
		TIWLAN_MMC_CONTROLLER_BASE_ADDR = OMAP_HSMMC2_BASE;
		TIWLAN_MMC_DMA_TX = OMAP24XX_DMA_MMC2_TX;
		TIWLAN_MMC_DMA_RX = OMAP24XX_DMA_MMC2_RX;
		OMAP_MMC_IRQ = INT_MMC2_IRQ;
	}
	else if( sdcnum == 3 ) {
		TIWLAN_MMC_CONTROLLER_BASE_ADDR	= OMAP_HSMMC3_BASE;
		TIWLAN_MMC_DMA_TX = OMAP34XX_DMA_MMC3_TX;
		TIWLAN_MMC_DMA_RX = OMAP34XX_DMA_MMC3_RX;
		OMAP_MMC_IRQ = INT_MMC3_IRQ;
	}
}
#endif

static void sdioDrv_hsmmc_save_ctx(void)
{
	/* MMC : context save */
	hsmmc_ctx.hctl = OMAP_HSMMC_READ(HCTL);
	hsmmc_ctx.capa = OMAP_HSMMC_READ(CAPA);
	hsmmc_ctx.sysconfig = OMAP_HSMMC_READ(SYSCONFIG);
	hsmmc_ctx.ise = OMAP_HSMMC_READ(ISE);
	hsmmc_ctx.ie = OMAP_HSMMC_READ(IE);
	hsmmc_ctx.con = OMAP_HSMMC_READ(CON);
	hsmmc_ctx.sysctl = OMAP_HSMMC_READ(SYSCTL);
	OMAP_HSMMC_WRITE(ISE, 0);
	OMAP_HSMMC_WRITE(IE, 0);
}

static void sdioDrv_hsmmc_restore_ctx(void)
{
        /* MMC : context restore */
        OMAP_HSMMC_WRITE(HCTL, hsmmc_ctx.hctl);
        OMAP_HSMMC_WRITE(CAPA, hsmmc_ctx.capa);
        OMAP_HSMMC_WRITE(SYSCONFIG, hsmmc_ctx.sysconfig);
        OMAP_HSMMC_WRITE(CON, hsmmc_ctx.con);
        OMAP_HSMMC_WRITE(ISE, hsmmc_ctx.ise);
        OMAP_HSMMC_WRITE(IE, hsmmc_ctx.ie);
        OMAP_HSMMC_WRITE(SYSCTL, hsmmc_ctx.sysctl);
        OMAP_HSMMC_WRITE(HCTL, OMAP_HSMMC_READ(HCTL) | SDBP);
}

static void sdioDrv_inact_timer(unsigned long data)
{
	sdioDrv_clk_disable();
	g_drv.inact_timer_running = 0;
}

void sdioDrv_start_inact_timer(void)
{
	mod_timer(&g_drv.inact_timer, jiffies + msecs_to_jiffies(1000));
	g_drv.inact_timer_running = 1;
}

void sdioDrv_cancel_inact_timer(void)
{
	if(g_drv.inact_timer_running) {
		del_timer_sync(&g_drv.inact_timer);
		g_drv.inact_timer_running = 0;
	}
}

void sdiodrv_task(struct work_struct *unused)
{
	PDEBUG("sdiodrv_tasklet()\n");

	if (g_drv.dma_read_addr != 0) {
		dma_unmap_single(g_drv.dev, g_drv.dma_read_addr, g_drv.dma_read_size, DMA_FROM_DEVICE);
		g_drv.dma_read_addr = 0;
		g_drv.dma_read_size = 0;
	}
	
	if (g_drv.dma_write_addr != 0) {
		dma_unmap_single(g_drv.dev, g_drv.dma_write_addr, g_drv.dma_write_size, DMA_TO_DEVICE);
		g_drv.dma_write_addr = 0;
		g_drv.dma_write_size = 0;
	}

	if (g_drv.BusTxnCB != NULL) {
		g_drv.BusTxnCB(g_drv.BusTxnHandle, g_drv.async_status);
	}
}

irqreturn_t sdiodrv_irq(int irq, void *drv)
{
	int status;

	PDEBUG("sdiodrv_irq()\n");

	status = OMAP_HSMMC_READ(STAT);
	OMAP_HSMMC_WRITE(ISE, 0);
	g_drv.async_status = status & (OMAP_HSMMC_ERR);
	if (g_drv.async_status) {
		PERR("sdiodrv_irq: ERROR in STAT = 0x%x\n", status);
	}
	queue_work(g_drv.sdio_wq, &g_drv.sdiodrv_work);
	return IRQ_HANDLED;
}

void sdiodrv_dma_read_cb(int lch, u16 ch_status, void *data)
{
	PDEBUG("sdiodrv_dma_read_cb() channel=%d status=0x%x\n", lch, (int)ch_status);

	g_drv.async_status = ch_status & (1 << 7);

	queue_work(g_drv.sdio_wq, &g_drv.sdiodrv_work);
	sdiodrv_dma_shutdown();
}

void sdiodrv_dma_write_cb(int lch, u16 ch_status, void *data)
{
	sdiodrv_dma_shutdown();
}

int sdiodrv_dma_init(void)
{
	int rc;

	rc = omap_request_dma(TIWLAN_MMC_DMA_TX, "SDIO WRITE", sdiodrv_dma_write_cb, &g_drv, &g_drv.dma_tx_channel);
	if (rc != 0) {
		PERR("sdiodrv_dma_init() omap_request_dma(TIWLAN_MMC_DMA_TX) FAILED\n");
		goto out;
	}

	rc = omap_request_dma(TIWLAN_MMC_DMA_RX, "SDIO READ", sdiodrv_dma_read_cb, &g_drv, &g_drv.dma_rx_channel);
	if (rc != 0) {
		PERR("sdiodrv_dma_init() omap_request_dma(TIWLAN_MMC_DMA_RX) FAILED\n");
		goto freetx;
	}

	omap_set_dma_src_params(g_drv.dma_rx_channel,
  							0,			// src_port is only for OMAP1
  							OMAP_DMA_AMODE_CONSTANT,
  							(TIWLAN_MMC_CONTROLLER_BASE_ADDR) + OMAP_HSMMC_DATA, 0, 0);
  
	omap_set_dma_dest_params(g_drv.dma_tx_channel,
							0,			// dest_port is only for OMAP1
  	  						OMAP_DMA_AMODE_CONSTANT,
  	  						(TIWLAN_MMC_CONTROLLER_BASE_ADDR) + OMAP_HSMMC_DATA, 0, 0);

	return 0;

freetx:
	omap_free_dma(g_drv.dma_tx_channel);
out:
	return rc;
}

static void sdiodrv_dma_shutdown(void)
{
	omap_free_dma(g_drv.dma_tx_channel);
	omap_free_dma(g_drv.dma_rx_channel);
} /* sdiodrv_dma_shutdown() */

static u32 sdiodrv_poll_status(u32 reg_offset, u32 stat, unsigned int msecs)
{
	u32 status=0, loops=0;

	do
	{
		status = OMAP_HSMMC_READ_OFFSET(reg_offset);
		if(( status & stat))
		{
			break;
		}
	} while (loops++ < SDIODRV_MAX_LOOPS);

	return status;
} /* sdiodrv_poll_status */

void dumpreg(void)
{
	printk(KERN_ERR "\n MMCHS_SYSCONFIG   for mmc3 = %x  ", omap_readl( 0x480AD010 ));
	printk(KERN_ERR "\n MMCHS_SYSSTATUS   for mmc3 = %x  ", omap_readl( 0x480AD014 ));
	printk(KERN_ERR "\n MMCHS_CSRE	      for mmc3 = %x  ", omap_readl( 0x480AD024 ));
	printk(KERN_ERR "\n MMCHS_SYSTEST     for mmc3 = %x  ", omap_readl( 0x480AD028 ));
	printk(KERN_ERR "\n MMCHS_CON         for mmc3 = %x  ", omap_readl( 0x480AD02C ));
	printk(KERN_ERR "\n MMCHS_PWCNT       for mmc3 = %x  ", omap_readl( 0x480AD030 ));
	printk(KERN_ERR "\n MMCHS_BLK         for mmc3 = %x  ", omap_readl( 0x480AD104 ));
	printk(KERN_ERR "\n MMCHS_ARG         for mmc3 = %x  ", omap_readl( 0x480AD108 ));
	printk(KERN_ERR "\n MMCHS_CMD         for mmc3 = %x  ", omap_readl( 0x480AD10C ));
	printk(KERN_ERR "\n MMCHS_RSP10       for mmc3 = %x  ", omap_readl( 0x480AD110 ));
	printk(KERN_ERR "\n MMCHS_RSP32       for mmc3 = %x  ", omap_readl( 0x480AD114 ));
	printk(KERN_ERR "\n MMCHS_RSP54       for mmc3 = %x  ", omap_readl( 0x480AD118 ));
	printk(KERN_ERR "\n MMCHS_RSP76       for mmc3 = %x  ", omap_readl( 0x480AD11C ));
	printk(KERN_ERR "\n MMCHS_DATA        for mmc3 = %x  ", omap_readl( 0x480AD120 ));
	printk(KERN_ERR "\n MMCHS_PSTATE      for mmc3 = %x  ", omap_readl( 0x480AD124 ));
	printk(KERN_ERR "\n MMCHS_HCTL        for mmc3 = %x  ", omap_readl( 0x480AD128 ));
	printk(KERN_ERR "\n MMCHS_SYSCTL      for mmc3 = %x  ", omap_readl( 0x480AD12C ));
	printk(KERN_ERR "\n MMCHS_STAT        for mmc3 = %x  ", omap_readl( 0x480AD130 ));
	printk(KERN_ERR "\n MMCHS_IE          for mmc3 = %x  ", omap_readl( 0x480AD134 ));
	printk(KERN_ERR "\n MMCHS_ISE         for mmc3 = %x  ", omap_readl( 0x480AD138 ));
	printk(KERN_ERR "\n MMCHS_AC12        for mmc3 = %x  ", omap_readl( 0x480AD13C ));
	printk(KERN_ERR "\n MMCHS_CAPA        for mmc3 = %x  ", omap_readl( 0x480AD140 ));
	printk(KERN_ERR "\n MMCHS_CUR_CAPA    for mmc3 = %x  ", omap_readl( 0x480AD148 ));
}

//cmd flow p. 3609 obc
static int sdiodrv_send_command(u32 cmdreg, u32 cmdarg)
{
	OMAP_HSMMC_WRITE(STAT, OMAP_HSMMC_STAT_CLEAR);
	OMAP_HSMMC_SEND_COMMAND(cmdreg, cmdarg);

	return sdiodrv_poll_status(OMAP_HSMMC_STAT, CC, MMC_TIMEOUT_MS);
} /* sdiodrv_send_command() */

/*
 *  Disable clock to the card
 */
static void OMAP3430_mmc_stop_clock(void)
{
	OMAP_HSMMC_WRITE(SYSCTL, OMAP_HSMMC_READ(SYSCTL) & ~CEN);
	if ((OMAP_HSMMC_READ(SYSCTL) & CEN) != 0x0)
    {
		PERR("MMC clock not stoped, clock freq can not be altered\n");
    }
} /* OMAP3430_mmc_stop_clock */

/*
 *  Reset the SD system
 */
int OMAP3430_mmc_reset(void)
{
	int status, loops=0;
	//p. 3598 - need to set SOFTRESET to 0x1 0bc
	OMAP_HSMMC_WRITE(SYSCTL, OMAP_HSMMC_READ(SYSCTL) | SRA);
	while ((status = OMAP_HSMMC_READ(SYSCTL) &  SRA) && loops++ < SDIODRV_MAX_LOOPS);
	if (status & SRA)
	{
	    PERR("OMAP3430_mmc_reset() MMC reset FAILED!! status=0x%x\n",status);
	}

	return status;

} /* OMAP3430_mmc_reset */

//p. 3611
static void OMAP3430_mmc_set_clock(unsigned int clock, OMAP3430_sdiodrv_t *host)
{
	u16           dsor = 0;
	unsigned long regVal;
	int           status;

	PDEBUG("OMAP3430_mmc_set_clock(%d)\n",clock);
	if (clock) {
		/* Enable MMC_SD_CLK */
		dsor = OMAP_MMC_MASTER_CLOCK / clock;
		if (dsor < 1) {
			dsor = 1;
		}
		if (OMAP_MMC_MASTER_CLOCK / dsor > clock) {
			dsor++;
		}
		if (dsor > 250) {
			dsor = 250;
		}
	}
	OMAP3430_mmc_stop_clock();
	regVal = OMAP_HSMMC_READ(SYSCTL);
	regVal = regVal & ~(CLKD_MASK);//p. 3652
	regVal = regVal | (dsor << 6);
	regVal = regVal | (DTO << 16);//data timeout
	OMAP_HSMMC_WRITE(SYSCTL, regVal);
	OMAP_HSMMC_WRITE(SYSCTL, OMAP_HSMMC_READ(SYSCTL) | ICE);//internal clock enable. obc not mentioned in the spec
	/* 
     * wait till the the clock is stable (ICS) bit is set
	 */
	status  = sdiodrv_poll_status(OMAP_HSMMC_SYSCTL, ICS, MMC_TIMEOUT_MS);
	if(!(status & ICS)) {
	    PERR("OMAP3430_mmc_set_clock() clock not stable!! status=0x%x\n",status);
	}
	/* 
	 * Enable clock to the card
	 */
	OMAP_HSMMC_WRITE(SYSCTL, OMAP_HSMMC_READ(SYSCTL) | CEN);

} /* OMAP3430_mmc_set_clock() */

static void sdiodrv_free_resources(void)
{
	sdioDrv_cancel_inact_timer();

	if(g_drv.ifclks_enabled) {
		sdioDrv_clk_disable();
	}

	if (sdiodrv_fclk_got) {
		clk_put(g_drv.fclk);
		sdiodrv_fclk_got = 0;
	}

	if (sdiodrv_iclk_got) {
		clk_put(g_drv.iclk);
		sdiodrv_iclk_got = 0;
	}

        if (sdiodrv_irq_requested) {
                free_irq(OMAP_MMC_IRQ, &g_drv);
                sdiodrv_irq_requested = 0;
        }
}

int sdioDrv_InitHw(void)
{
	int rc;
	u32 status;
#ifdef SDIO_1_BIT /* see also in SdioAdapter.c */
	unsigned long clock_rate = 6000000;
#else
	unsigned long clock_rate = 24000000;
#endif

	printk(KERN_INFO "TIWLAN SDIO sdioDrv_InitHw()!!");

        rc = sdioDrv_clk_enable();
	if (rc) {
		PERR("sdioDrv_InitHw : sdioDrv_clk_enable FAILED !!!\n");
		goto err;
	}

	OMAP3430_mmc_reset();

	//obc - init sequence p. 3600,3617
	/* 1.8V */
	OMAP_HSMMC_WRITE(CAPA, OMAP_HSMMC_READ(CAPA) | VS18);
	OMAP_HSMMC_WRITE(HCTL, OMAP_HSMMC_READ(HCTL) | SDVS18);//SDVS fits p. 3650
	/* clock gating */
	OMAP_HSMMC_WRITE(SYSCONFIG, OMAP_HSMMC_READ(SYSCONFIG) | AUTOIDLE);

	/* bus power */
	OMAP_HSMMC_WRITE(HCTL, OMAP_HSMMC_READ(HCTL) | SDBP);//SDBP fits p. 3650
	/* interrupts */
	OMAP_HSMMC_WRITE(ISE, 0);
	OMAP_HSMMC_WRITE(IE, IE_EN_MASK);

	//p. 3601 suggests moving to the end
	OMAP3430_mmc_set_clock(clock_rate, &g_drv);
	printk("SDIO clock Configuration is now set to %dMhz\n",(int)clock_rate/1000000);

	/* Bus width */
#ifdef SDIO_1_BIT /* see also in SdioAdapter.c */
	PDEBUG("%s() setting %d data lines\n",__FUNCTION__, 1);
	OMAP_HSMMC_WRITE(HCTL, OMAP_HSMMC_READ(HCTL) & (ONE_BIT));
#else
	PDEBUG("%s() setting %d data lines\n",__FUNCTION__, 4);
	OMAP_HSMMC_WRITE(HCTL, OMAP_HSMMC_READ(HCTL) | (1 << 1));//DTW 4 bits - p. 3650
#endif

	/* send the init sequence. 80 clocks of synchronization in the SDIO */
	//doesn't match p. 3601,3617 - obc
	OMAP_HSMMC_WRITE( CON, OMAP_HSMMC_READ(CON) | INIT_STREAM);
	OMAP_HSMMC_SEND_COMMAND( 0, 0);
	status = sdiodrv_poll_status(OMAP_HSMMC_STAT, CC, MMC_TIMEOUT_MS);
	if (!(status & CC)) {
		PERR("sdioDrv_InitHw() SDIO Command error status = 0x%x\n", status);
		rc = status;
		goto err;
	}
	OMAP_HSMMC_WRITE(CON, OMAP_HSMMC_READ(CON) & ~INIT_STREAM);

	return 0;

err:
	/* Disabling clocks for now */
	sdioDrv_clk_disable();

	return rc;

} /* sdiodrv_InitHw */

void sdiodrv_shutdown(void)
{
	PDEBUG("entering %s()\n" , __FUNCTION__ );

	sdiodrv_free_resources();

	PDEBUG("exiting %s\n", __FUNCTION__);
} /* sdiodrv_shutdown() */

static int sdiodrv_send_data_xfer_commad(u32 cmd, u32 cmdarg, int length, u32 buffer_enable_status, unsigned int bBlkMode)
{
    int status;

	PDEBUG("%s() writing CMD 0x%x ARG 0x%x\n",__FUNCTION__, cmd, cmdarg);

    /* block mode */
	if(bBlkMode) {
        /* 
         * Bits 31:16 of BLK reg: NBLK Blocks count for current transfer.
         *                        in case of Block MOde the lenght is treated here as number of blocks 
         *                        (and not as a length).
         * Bits 11:0 of BLK reg: BLEN Transfer Block Size. in case of block mode set that field to block size. 
         */
        OMAP_HSMMC_WRITE(BLK, (length << 16) | (g_drv.uBlkSize << 0));

        /*
         * In CMD reg:
         * BCE: Block Count Enable
         * MSBS: Multi/Single block select
         */
        cmd |= MSBS | BCE ;
	} else {
        OMAP_HSMMC_WRITE(BLK, length);
    }

    status = sdiodrv_send_command(cmd, cmdarg);
	if(!(status & CC)) {
	    PERR("sdiodrv_send_data_xfer_commad() SDIO Command error! STAT = 0x%x\n", status);
	    return 0;
	}
	PDEBUG("%s() length = %d(%dw) BLK = 0x%x\n",
		   __FUNCTION__, length,((length + 3) >> 2), OMAP_HSMMC_READ(BLK));

    return sdiodrv_poll_status(OMAP_HSMMC_PSTATE, buffer_enable_status, MMC_TIMEOUT_MS);

} /* sdiodrv_send_data_xfer_commad() */

int sdiodrv_data_xfer_sync(u32 cmd, u32 cmdarg, void *data, int length, u32 buffer_enable_status)
{
    u32 buf_start, buf_end, data32;
	int status;

    status = sdiodrv_send_data_xfer_commad(cmd, cmdarg, length, buffer_enable_status, 0);
	if(!(status & buffer_enable_status)) 
    {
	    PERR("sdiodrv_data_xfer_sync() buffer disabled! length = %d BLK = 0x%x PSTATE = 0x%x\n", 
			   length, OMAP_HSMMC_READ(BLK), status);
	    return -1;
	}
	buf_end = (u32)data+(u32)length;

	//obc need to check BRE/BWE every time, see p. 3605
	/*
	 * Read loop 
	 */
	if (buffer_enable_status == BRE)
	{
	  if (((u32)data & 3) == 0) /* 4 bytes aligned */
	  {
		for (buf_start = (u32)data; (u32)data < buf_end; data += sizeof(unsigned long))
		{
		  *((unsigned long*)(data)) = OMAP_HSMMC_READ(DATA);
		}
	  }
	  else                      /* 2 bytes aligned */
	  {
		for (buf_start = (u32)data; (u32)data < buf_end; data += sizeof(unsigned long))
		{
		  data32 = OMAP_HSMMC_READ(DATA);
		  *((unsigned short *)data)     = (unsigned short)data32;
		  *((unsigned short *)data + 1) = (unsigned short)(data32 >> 16);
		}
	  }
	}
	/*
	 * Write loop 
	 */
	else
	{
	  if (((u32)data & 3) == 0) /* 4 bytes aligned */
	  {
		for (buf_start = (u32)data; (u32)data < buf_end; data += sizeof(unsigned long))
		{
		  OMAP_HSMMC_WRITE(DATA,*((unsigned long*)(data)));
		}
	  }
	  else                      /* 2 bytes aligned */
	  {
		for (buf_start = (u32)data; (u32)data < buf_end; data += sizeof(unsigned long))
		{
		  OMAP_HSMMC_WRITE(DATA,*((unsigned short*)data) | *((unsigned short*)data+1) << 16 );
		}

	  }
	}
	status  = sdiodrv_poll_status(OMAP_HSMMC_STAT, TC, MMC_TIMEOUT_MS);
	if(!(status & TC)) 
	{
	    PERR("sdiodrv_data_xfer_sync() transfer error! STAT = 0x%x\n", status);
	    return -1;
	}

	return 0;

} /* sdiodrv_data_xfer_sync() */

int sdioDrv_ConnectBus (void *       fCbFunc,
                        void *       hCbArg,
                        unsigned int uBlkSizeShift,
                        unsigned int  uSdioThreadPriority)
{
	g_drv.BusTxnCB      = fCbFunc;
	g_drv.BusTxnHandle  = hCbArg;
	g_drv.uBlkSizeShift = uBlkSizeShift;  
	g_drv.uBlkSize      = 1 << uBlkSizeShift;

	INIT_WORK(&g_drv.sdiodrv_work, sdiodrv_task);

	return sdioDrv_InitHw ();
}

/*--------------------------------------------------------------------------------------*/

int sdioDrv_DisconnectBus (void)
{
	/* Disable clocks to handle driver stop command */
	sdioDrv_clk_disable();
	return 0;
}

//p.3609 cmd flow
int sdioDrv_ExecuteCmd (unsigned int uCmd, 
                        unsigned int uArg, 
                        unsigned int uRespType, 
                        void *       pResponse, 
                        unsigned int uLen)
{
	unsigned int uCmdReg   = 0;
	unsigned int uStatus   = 0;
	unsigned int uResponse = 0;

	PDEBUG("sdioDrv_ExecuteCmd() starting cmd %02x arg %08x\n", (int)uCmd, (int)uArg);

	sdioDrv_clk_enable(); /* To make sure we have clocks enable */

	uCmdReg = (uCmd << 24) | (uRespType << 16) ;

	uStatus = sdiodrv_send_command(uCmdReg, uArg);

	if (!(uStatus & CC)) 
	{
	    PERR("sdioDrv_ExecuteCmd() SDIO Command error status = 0x%x\n", uStatus);
	    return -1;
	}
	if ((uLen > 0) && (uLen <= 4))/*obc - Len > 4 ? shouldn't read anything ? */
	{
	    uResponse = OMAP_HSMMC_READ(RSP10);
		memcpy (pResponse, (char *)&uResponse, uLen);
		PDEBUG("sdioDrv_ExecuteCmd() response = 0x%x\n", uResponse);
	}
	return 0;
}

/*--------------------------------------------------------------------------------------*/

int sdioDrv_ReadSync (unsigned int uFunc, 
                      unsigned int uHwAddr, 
                      void *       pData, 
                      unsigned int uLen,
                      unsigned int bIncAddr,
                      unsigned int bMore)
{
	unsigned int uCmdArg;
	int          iStatus;

//	printk(KERN_INFO "in sdioDrv_ReadSync\n");
	uCmdArg = SDIO_CMD53_READ(0, uFunc, 0, bIncAddr, uHwAddr, uLen);

	iStatus = sdiodrv_data_xfer_sync(OMAP_HSMMC_CMD53_READ, uCmdArg, pData, uLen, BRE);
	if (iStatus != 0) {
		PERR("sdioDrv_ReadSync() FAILED!!\n");
	}
#ifdef TI_SDIO_DEBUG
	if (uLen == 1)
		printk(KERN_INFO "R53: [0x%x](%u) = 0x%x\n", uHwAddr, uLen, (unsigned)(*(char *)pData));
	else if (uLen == 2)
		printk(KERN_INFO "R53: [0x%x](%u) = 0x%x\n", uHwAddr, uLen, (unsigned)(*(short *)pData));
	else if (uLen == 4)
		printk(KERN_INFO "R53: [0x%x](%u) = 0x%x\n", uHwAddr, uLen, (unsigned)(*(long *)pData));
	else
		printk(KERN_INFO "R53: [0x%x](%u)\n", uHwAddr, uLen);
#endif
	return iStatus;
}

/*--------------------------------------------------------------------------------------*/
int sdioDrv_ReadAsync (unsigned int uFunc, 
                       unsigned int uHwAddr, 
                       void *       pData, 
                       unsigned int uLen, 
                       unsigned int bBlkMode,
                       unsigned int bIncAddr,
                       unsigned int bMore)
{
	int          iStatus;
	unsigned int uCmdArg;
	unsigned int uNumBlks;
	unsigned int uDmaBlockCount;
	unsigned int uNumOfElem;
	dma_addr_t dma_bus_address;

#ifdef TI_SDIO_DEBUG
	printk(KERN_INFO "R53: [0x%x](%u) F[%d]\n", uHwAddr, uLen, uFunc);
#endif

	//printk(KERN_INFO "in sdioDrv_ReadAsync\n");

    if (bBlkMode)
    {
        /* For block mode use number of blocks instead of length in bytes */
        uNumBlks = uLen >> g_drv.uBlkSizeShift;
        uDmaBlockCount = uNumBlks;
        /* due to the DMA config to 32Bit per element (OMAP_DMA_DATA_TYPE_S32) the division is by 4 */ 
        uNumOfElem = g_drv.uBlkSize >> 2;
    }
    else
    {	
        uNumBlks = uLen;
        uDmaBlockCount = 1;
        uNumOfElem = (uLen + 3) >> 2;
    }

    uCmdArg = SDIO_CMD53_READ(0, uFunc, bBlkMode, bIncAddr, uHwAddr, uNumBlks);

    iStatus = sdiodrv_send_data_xfer_commad(OMAP_HSMMC_CMD53_READ_DMA, uCmdArg, uNumBlks, BRE, bBlkMode);

    if (!(iStatus & BRE)) 
    {
        PERR("sdioDrv_ReadAsync() buffer disabled! length = %d BLK = 0x%x PSTATE = 0x%x, BlkMode = %d\n", 
              uLen, OMAP_HSMMC_READ(BLK), iStatus, bBlkMode);
	goto err;
    }

	sdiodrv_dma_init();

	PDEBUG("sdiodrv_read_async() dma_ch=%d \n",g_drv.dma_rx_channel);

	dma_bus_address = dma_map_single(g_drv.dev, pData, uLen, DMA_FROM_DEVICE);
	if (!dma_bus_address) {
		PERR("sdioDrv_ReadAsync: dma_map_single failed\n");
		goto err;
	}		

	if (g_drv.dma_read_addr != 0) {
		printk(KERN_ERR "sdioDrv_ReadAsync: previous DMA op is not finished!\n");
		BUG();
	}
	
	g_drv.dma_read_addr = dma_bus_address;
	g_drv.dma_read_size = uLen;

	omap_set_dma_dest_params    (g_drv.dma_rx_channel,
									0,			// dest_port is only for OMAP1
									OMAP_DMA_AMODE_POST_INC,
									dma_bus_address,
									0, 0);

	omap_set_dma_transfer_params(g_drv.dma_rx_channel, OMAP_DMA_DATA_TYPE_S32, uNumOfElem , uDmaBlockCount , OMAP_DMA_SYNC_FRAME, TIWLAN_MMC_DMA_RX, OMAP_DMA_SRC_SYNC);

	omap_start_dma(g_drv.dma_rx_channel);

	/* Continued at sdiodrv_irq() after DMA transfer is finished */
#ifdef TI_SDIO_DEBUG
	printk(KERN_INFO "R53: [0x%x](%u) (A)\n", uHwAddr, uLen);
#endif
	return 0;
err:
	return -1;

}


/*--------------------------------------------------------------------------------------*/

int sdioDrv_WriteSync (unsigned int uFunc, 
                       unsigned int uHwAddr, 
                       void *       pData, 
                       unsigned int uLen,
                       unsigned int bIncAddr,
                       unsigned int bMore)
{
	unsigned int uCmdArg;
	int          iStatus;

//	printk(KERN_INFO "in sdioDrv_WriteSync\n");

	uCmdArg = SDIO_CMD53_WRITE(1, uFunc, 0, bIncAddr, uHwAddr, uLen);

	iStatus = sdiodrv_data_xfer_sync(OMAP_HSMMC_CMD53_WRITE, uCmdArg, pData, uLen, BWE);
	if (iStatus != 0)
	{
		PERR("sdioDrv_WriteSync() FAILED!!\n");
	}
#ifdef TI_SDIO_DEBUG
	if (uLen == 1)
		printk(KERN_INFO "W53: [0x%x](%u) < 0x%x\n", uHwAddr, uLen, (unsigned)(*(char *)pData));
	else if (uLen == 2)
		printk(KERN_INFO "W53: [0x%x](%u) < 0x%x\n", uHwAddr, uLen, (unsigned)(*(short *)pData));
	else if (uLen == 4)
		printk(KERN_INFO "W53: [0x%x](%u) < 0x%x\n", uHwAddr, uLen, (unsigned)(*(long *)pData));
	else
		printk(KERN_INFO "W53: [0x%x](%u)\n", uHwAddr, uLen);
#endif
	return iStatus;
}

/*--------------------------------------------------------------------------------------*/
int sdioDrv_WriteAsync (unsigned int uFunc, 
                        unsigned int uHwAddr, 
                        void *       pData, 
                        unsigned int uLen, 
                        unsigned int bBlkMode,
                        unsigned int bIncAddr,
                        unsigned int bMore)
{
	int          iStatus;
	unsigned int uCmdArg;
	unsigned int uNumBlks;
	unsigned int uDmaBlockCount;
	unsigned int uNumOfElem;
	dma_addr_t dma_bus_address;

#ifdef TI_SDIO_DEBUG
	printk(KERN_INFO "W53: [0x%x](%u) F[%d] B[%d] I[%d]\n", uHwAddr, uLen, uFunc, bBlkMode, bIncAddr);
#endif

//	printk(KERN_INFO "in sdioDrv_WriteAsync\n");
    if (bBlkMode)
    {
        /* For block mode use number of blocks instead of length in bytes */
        uNumBlks = uLen >> g_drv.uBlkSizeShift;
        uDmaBlockCount = uNumBlks;
        /* due to the DMA config to 32Bit per element (OMAP_DMA_DATA_TYPE_S32) the division is by 4 */ 
        uNumOfElem = g_drv.uBlkSize >> 2;
    }
    else
    {
        uNumBlks = uLen;
        uDmaBlockCount = 1;
        uNumOfElem = (uLen + 3) >> 2;
    }

    uCmdArg = SDIO_CMD53_WRITE(1, uFunc, bBlkMode, bIncAddr, uHwAddr, uNumBlks);

    iStatus = sdiodrv_send_data_xfer_commad(OMAP_HSMMC_CMD53_WRITE_DMA, uCmdArg, uNumBlks, BWE, bBlkMode);
    if (!(iStatus & BWE)) 
    {
        PERR("sdioDrv_WriteAsync() buffer disabled! length = %d, BLK = 0x%x, Status = 0x%x\n", 
             uLen, OMAP_HSMMC_READ(BLK), iStatus);
	goto err;
    }

	OMAP_HSMMC_WRITE(ISE, TC);

	sdiodrv_dma_init();

	dma_bus_address = dma_map_single(g_drv.dev, pData, uLen, DMA_TO_DEVICE);
	if (!dma_bus_address) {
		PERR("sdioDrv_WriteAsync: dma_map_single failed\n");
		goto err;
	}

	if (g_drv.dma_write_addr != 0) {
		PERR("sdioDrv_WriteAsync: previous DMA op is not finished!\n");
		BUG();
	}
	
	g_drv.dma_write_addr = dma_bus_address;
	g_drv.dma_write_size = uLen;

	omap_set_dma_src_params     (g_drv.dma_tx_channel,
									0,			// src_port is only for OMAP1
									OMAP_DMA_AMODE_POST_INC,
									dma_bus_address,
									0, 0);

	omap_set_dma_transfer_params(g_drv.dma_tx_channel, OMAP_DMA_DATA_TYPE_S32, uNumOfElem, uDmaBlockCount, OMAP_DMA_SYNC_FRAME, TIWLAN_MMC_DMA_TX, OMAP_DMA_DST_SYNC);

	omap_start_dma(g_drv.dma_tx_channel);

	/* Continued at sdiodrv_irq() after DMA transfer is finished */
	return 0;
err:
	return -1;
}

/*--------------------------------------------------------------------------------------*/

int sdioDrv_ReadSyncBytes (unsigned int  uFunc, 
                           unsigned int  uHwAddr, 
                           unsigned char *pData, 
                           unsigned int  uLen, 
                           unsigned int  bMore)
{
	unsigned int uCmdArg;
	unsigned int i;
	int          iStatus;

	for (i = 0; i < uLen; i++) {
		uCmdArg = SDIO_CMD52_READ(0, uFunc, 0, uHwAddr);

		iStatus = sdiodrv_send_command(OMAP_HSMMC_CMD52_READ, uCmdArg);

		if (!(iStatus & CC)) {
			PERR("sdioDrv_ReadSyncBytes() SDIO Command error status = 0x%x\n", iStatus);
			return -1;
		}
		else {
			*pData = (unsigned char)(OMAP_HSMMC_READ(RSP10));
		}
#ifdef TI_SDIO_DEBUG
		printk(KERN_INFO "R52: [0x%x](%u) = 0x%x\n", uHwAddr, uLen, (unsigned)*pData);
#endif
		uHwAddr++;
		pData++;
	}

	return 0;
}

/*--------------------------------------------------------------------------------------*/

int sdioDrv_WriteSyncBytes (unsigned int  uFunc, 
                            unsigned int  uHwAddr, 
                            unsigned char *pData, 
                            unsigned int  uLen, 
                            unsigned int  bMore)
{
	unsigned int uCmdArg;
	unsigned int i;
	int          iStatus;

	for (i = 0; i < uLen; i++) {
#ifdef TI_SDIO_DEBUG
		printk(KERN_INFO "W52: [0x%x](%u) < 0x%x\n", uHwAddr, uLen, (unsigned)*pData);
#endif
		uCmdArg = SDIO_CMD52_WRITE(1, uFunc, 0, uHwAddr, *pData);

		iStatus = sdiodrv_send_command(OMAP_HSMMC_CMD52_WRITE, uCmdArg);
		if (!(iStatus & CC)) {
			PERR("sdioDrv_WriteSyncBytes() SDIO Command error status = 0x%x\n", iStatus);
			return -1;
		}
		uHwAddr++;
		pData++;
	}

	return 0;
}

static int sdioDrv_probe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "TIWLAN SDIO probe: initializing mmc%d device\n", pdev->id + 1);

	/* remember device struct for future DMA operations */
	g_drv.dev = &pdev->dev;
	g_drv.irq = platform_get_irq(pdev, 0);
	if (g_drv.irq < 0)
		return -ENXIO;

        rc= request_irq(OMAP_MMC_IRQ, sdiodrv_irq, 0, SDIO_DRIVER_NAME, &g_drv);
        if (rc != 0) {
                PERR("sdioDrv_probe() - request_irq FAILED!!\n");
                return rc;
        }
        sdiodrv_irq_requested = 1;

	spin_lock_init(&g_drv.clk_lock);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 31))
	dummy_pdev.id = TIWLAN_MMC_CONTROLLER;
	dev_set_name(&dummy_pdev.dev, "mmci-omap-hs.%lu", TIWLAN_MMC_CONTROLLER);
	g_drv.fclk = clk_get(&dummy_pdev.dev, "fck");
#else
	g_drv.fclk = clk_get(&pdev->dev, "mmchs_fck");
#endif
	if (IS_ERR(g_drv.fclk)) {
		rc = PTR_ERR(g_drv.fclk);
		PERR("clk_get(fclk) FAILED !!!\n");
		goto err;
	}
	sdiodrv_fclk_got = 1;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 31))
	g_drv.iclk = clk_get(&dummy_pdev.dev, "ick");
#else
	g_drv.iclk = clk_get(&pdev->dev, "mmchs_ick");
#endif
	if (IS_ERR(g_drv.iclk)) {
		rc = PTR_ERR(g_drv.iclk);
		PERR("clk_get(iclk) FAILED !!!\n");
		goto err;
	}
	sdiodrv_iclk_got = 1;
	
	printk("Configuring SDIO DMA registers!!!\n");
	printk("pdev->id is %d!!!\n", pdev->id);
	if ( pdev->id == 1 ) {
		/* MMC2 */
		TIWLAN_MMC_CONTROLLER_BASE_ADDR = OMAP_HSMMC2_BASE;
		TIWLAN_MMC_DMA_TX = OMAP24XX_DMA_MMC2_TX;
		TIWLAN_MMC_DMA_RX = OMAP24XX_DMA_MMC2_RX;
        }
	else if ( pdev->id == 2 ) {
		/* MMC3 */
		TIWLAN_MMC_CONTROLLER_BASE_ADDR	= OMAP_HSMMC3_BASE;
		TIWLAN_MMC_DMA_TX = OMAP34XX_DMA_MMC3_TX;
		TIWLAN_MMC_DMA_RX = OMAP34XX_DMA_MMC3_RX;
	}

	/* inactivity timer initialization*/
	init_timer(&g_drv.inact_timer);
	g_drv.inact_timer.function = sdioDrv_inact_timer;
	g_drv.inact_timer_running = 0;

	return 0;
err:
	sdiodrv_free_resources();
	return rc;
}

static int sdioDrv_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "sdioDrv_remove: calling sdiodrv_shutdown\n");
	
	sdiodrv_shutdown();
	
	return 0;
}

#ifdef CONFIG_PM
static int sdioDrv_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk(KERN_INFO "TISDIO: sdioDrv is suspending\n");
	return 0;
}

/* Routine to resume the MMC device */
static int sdioDrv_resume(struct platform_device *pdev)
{
	printk(KERN_INFO "TISDIO: sdioDrv is resuming\n");
	return 0;
}
#else
#define sdioDrv_suspend		NULL
#define sdioDrv_resume		NULL
#endif

static struct platform_driver sdioDrv_struct = {
	.probe		= sdioDrv_probe,
	.remove		= sdioDrv_remove,
	.suspend	= sdioDrv_suspend,
	.resume		= sdioDrv_resume,
	.driver		= {
		.name = SDIO_DRIVER_NAME,
	},
};

void sdioDrv_register_pm(int (*wlanDrvIf_Start)(void),
						int (*wlanDrvIf_Stop)(void))
{
	g_drv.wlanDrvIf_pm_resume = wlanDrvIf_Start;
	g_drv.wlanDrvIf_pm_suspend = wlanDrvIf_Stop;
}

int sdioDrv_clk_enable(void)
{
       unsigned long flags;
       int ret = 0;

       spin_lock_irqsave(&g_drv.clk_lock, flags);
       if (g_drv.ifclks_enabled)
               goto done;

       ret = clk_enable(g_drv.iclk);
       if (ret)
              goto clk_en_err1;

       ret = clk_enable(g_drv.fclk);
       if (ret)
               goto clk_en_err2;
       g_drv.ifclks_enabled = 1;

       sdioDrv_hsmmc_restore_ctx();

done:
       spin_unlock_irqrestore(&g_drv.clk_lock, flags);
       return ret;

clk_en_err2:
       clk_disable(g_drv.iclk);
clk_en_err1 :
       spin_unlock_irqrestore(&g_drv.clk_lock, flags);
       return ret;
}

void sdioDrv_clk_disable(void)
{
       unsigned long flags;

       spin_lock_irqsave(&g_drv.clk_lock, flags);
       if (!g_drv.ifclks_enabled)
               goto done;

       sdioDrv_hsmmc_save_ctx();

       clk_disable(g_drv.fclk);
       clk_disable(g_drv.iclk);
       g_drv.ifclks_enabled = 0;
done:
       spin_unlock_irqrestore(&g_drv.clk_lock, flags);
}

#ifdef TI_SDIO_STANDALONE
static int __init sdioDrv_init(void)
#else
int __init sdioDrv_init(int sdcnum)
#endif
{
	memset(&g_drv, 0, sizeof(g_drv));
	memset(&hsmmc_ctx, 0, sizeof(hsmmc_ctx));

	printk(KERN_INFO "TIWLAN SDIO init\n");
#ifndef TI_SDIO_STANDALONE
	sdio_init( sdcnum );
#endif
	g_drv.sdio_wq = create_freezeable_workqueue(SDIOWQ_NAME);
	if (!g_drv.sdio_wq) {
		printk("TISDIO: Fail to create SDIO WQ\n");
		return -EINVAL;
	}
	/* Register the sdio driver */
	return platform_driver_register(&sdioDrv_struct);
}

#ifdef TI_SDIO_STANDALONE
static
#endif
void __exit sdioDrv_exit(void)
{
	/* Unregister sdio driver */
	platform_driver_unregister(&sdioDrv_struct);
	if (g_drv.sdio_wq)
		destroy_workqueue(g_drv.sdio_wq);
}

#ifdef TI_SDIO_STANDALONE
module_init(sdioDrv_init);
module_exit(sdioDrv_exit);
#endif

EXPORT_SYMBOL(sdioDrv_ConnectBus);
EXPORT_SYMBOL(sdioDrv_DisconnectBus);
EXPORT_SYMBOL(sdioDrv_ExecuteCmd);
EXPORT_SYMBOL(sdioDrv_ReadSync);
EXPORT_SYMBOL(sdioDrv_WriteSync);
EXPORT_SYMBOL(sdioDrv_ReadAsync);
EXPORT_SYMBOL(sdioDrv_WriteAsync);
EXPORT_SYMBOL(sdioDrv_ReadSyncBytes);
EXPORT_SYMBOL(sdioDrv_WriteSyncBytes);
EXPORT_SYMBOL(sdioDrv_register_pm);
MODULE_DESCRIPTION("TI WLAN SDIO driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS(SDIO_DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
#endif
