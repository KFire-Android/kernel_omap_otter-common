/*
 * Copyright (C) ST-Ericsson AB 2009
 * Author:	Daniel Martensson / Daniel.Martensson@stericsson.com
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/version.h>
#include <linux/wakelock.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif /* KERN_VERSION_2_6_27 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
#include <plat/dma.h>
#include <plat/gpio.h>
#include <plat/clock.h>
#else
#include <mach/dma.h>
#include <mach/gpio.h>
#include <mach/clock.h>
#endif /* KERN_VERSION_2_6_32 */

#include <net/caif/caif_chr.h>
#include <net/caif/caif_spi.h>

MODULE_LICENSE("GPL");

/* Use McSPI2. */
#define PHYIF_SPI_NR			2

/* McSPI2 registers. */
#define OMAP2_MCSPI2_BASE_ADR		0x4809A000
#define OMAP2_MCSPI2_TX0_ADR		0x4809A038
#define OMAP2_MCSPI2_RX0_ADR		0x4809A03C

#define OMAP2_MCSPI_SYSCONFIG_OFS	0x10
#define OMAP2_MCSPI_SYSSTATUS_OFS	0x14
#define OMAP2_MCSPI_IRQSTATUS_OFS	0x18
#define OMAP2_MCSPI_WAKEUPENABLE_OFS	0x20
#define OMAP2_MCSPI_CH0CONF_OFS		0x2C
#define OMAP2_MCSPI_CH0STAT_OFS		0x30
#define OMAP2_MCSPI_CH0CTRL_OFS		0x34
#define OMAP2_MCSPI_XFERLEVEL_OFS	0x7C

/* McSPI2 SYSCONFIG bit fields. */
#define OMAP2_MCSPI_SYSCONFIG_SMARTIDLE 	(1 << 4)
#define OMAP2_MCSPI_SYSCONFIG_ENAWAKEUP 	(1 << 2)
#define OMAP2_MCSPI_SYSCONFIG_AUTOIDLE 		(1 << 0)
#define OMAP2_MCSPI_SYSCONFIG_SOFTRESET 	(1 << 1)
#define OMAP2_MCSPI_SYSCONFIG_IDLEMODE(x)	((x & 0x3) << 3)

/* McSPI2 SYSSTATUS bit fields. */
#define OMAP2_MCSPI_SYSSTATUS_RESETDONE (1 << 0)

/* McSPI2 IRQSTATUS bit fields. */
#define OMAP2_MCSPI_IRQSTATUS_TX0UFLOW	(1 << 1)
#define OMAP2_MCSPI_IRQSTATUS_RX0FULL	(1 << 2)
#define OMAP2_MCSPI_IRQSTATUS_RX0OFLOW	(1 << 3)

/* McSPI2 CHCONF bit fields. */
#define OMAP2_MCSPI_CHCONF_EPOL         (1 << 6)
#define OMAP2_MCSPI_CHCONF_WL(x)	(((x - 1) & 0x1F) << 7)
#define OMAP2_MCSPI_CHCONF_DMAW         (1 << 14)
#define OMAP2_MCSPI_CHCONF_DMAR         (1 << 15)
#define OMAP2_MCSPI_CHCONF_DPE1         (1 << 17)
#define OMAP2_MCSPI_CHCONF_IS		(1 << 18)
#define OMAP2_MCSPI_CHCONF_FFEW		(1 << 27)
#define OMAP2_MCSPI_CHCONF_FFER		(1 << 28)

/* McSPI2 CHCTRL bit fields. */
#define OMAP2_MCSPI_CHCTRL_EN           (1 << 0)

/* McSPI2 XFERLEVEL bit fields. */
#define OMAP2_MCSPI_XFERLEVEL_AEL(x)	(((x - 1) & 0x3F) << 0)
#define OMAP2_MCSPI_XFERLEVEL_AFL(x)	(((x - 1) & 0x3F) << 8)

#define OMAP2_MCSPI_WAKEUPENABLE_WKEN	(1 << 0)

#if 0
/* GPIOs used on the soldered on EMP modem. */
#define GPIO_MODEM_SPI_SS			105
#define GPIO_MODEM_SPI_INT		104
#endif

#if !defined(GPIO_MODEM_SPI_SS)
/* ZOOM2 TI Adaptor board defaults */
#define GPIO_MODEM_SPI_SS		159
#endif

#if !defined(GPIO_MODEM_SPI_INT)
/* ZOOM2 TI Adaptor board defaults */
#define GPIO_MODEM_SPI_INT		14
#endif

/* SS is active low. */
#define SS_CHANGED		(IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING)

/* SPI_INT is active high. */
#define SPI_INT_ACTIVE		1
#define SPI_INT_INACTIVE	0

/* Wordlength is 16 bit (2 bytes)*/
#define SIZE_OF_WL		2

struct omap_cfspi_struct {
	struct cfspi_dev sdev;
	struct cfspi_xfer *xfer;
	struct platform_device pdev;
	struct wake_lock wake_lock_spi_ss;
	struct wake_lock wake_lock_spi_int;
	int irq;
	int tx_chnl;
	int rx_chnl;
	struct omap_dma_channel_params tx_par;
	struct omap_dma_channel_params rx_par;
	struct completion dma_tx_completion;
	struct completion dma_rx_completion;
	__u16 dma_len;
	struct clk *ick;
	struct clk *fck;
	void __iomem *io;
	struct semaphore sem;
};

static struct omap_cfspi_struct cfspi;

/* used for context save and restore, structure members to be updated whenever
 * corresponding registers are modified.
 */
struct cfspi_regs {
	u32 sysconfig;
	u32 wakeupenable;
	u32 xfer_level;
	u32 channel_conf;
};

static struct cfspi_regs context;

static int cfspi_spi_clocks_enable(void)
{
	int res = clk_enable(cfspi.ick);
	if (res < 0) {
		printk(KERN_ERR
		      "omap2_cfspi: err: %d, can't enable iclk.\n", res);
		return -ENODEV;
	}
	res = clk_enable(cfspi.fck);
	if (res < 0) {
		printk(KERN_ERR
		      "omap2_cfspi: err: %d, can't enable fclk.\n", res);
		return -ENODEV;
	}
	return 0;
}

static void cfspi_spi_clocks_disable(void)
{
	clk_disable(cfspi.ick);
	clk_disable(cfspi.fck);
}

static int cfspi_spi_enable(void)
{
	int res = cfspi_spi_clocks_enable();

	/* We only have 0 after reset */
	if (__raw_readl(cfspi.io + OMAP2_MCSPI_CH0CONF_OFS) !=
							context.channel_conf) {
		printk(KERN_INFO "McSPI2 soft reset\n");
		/* Reset SPI module. */
		__raw_writel(OMAP2_MCSPI_SYSCONFIG_SOFTRESET,
			cfspi.io + OMAP2_MCSPI_SYSCONFIG_OFS);

		/* Wait for reset to finish. */
		while (!(__raw_readl(cfspi.io + OMAP2_MCSPI_SYSSTATUS_OFS) &
					OMAP2_MCSPI_SYSSTATUS_RESETDONE))
			continue;

		__raw_writel(context.sysconfig,
			cfspi.io + OMAP2_MCSPI_SYSCONFIG_OFS);

		__raw_writel(context.wakeupenable,
			cfspi.io + OMAP2_MCSPI_WAKEUPENABLE_OFS);

		__raw_writel(context.xfer_level,
			cfspi.io + OMAP2_MCSPI_XFERLEVEL_OFS);

		__raw_writel(context.channel_conf,
			cfspi.io + OMAP2_MCSPI_CH0CONF_OFS);
	}

	/* Enable */
	__raw_writel(OMAP2_MCSPI_CHCTRL_EN, cfspi.io + OMAP2_MCSPI_CH0CTRL_OFS);

	return res;
}

void cfspi_spi_disable(void)
{
	__raw_writel(0, cfspi.io + OMAP2_MCSPI_CH0CTRL_OFS);
	cfspi_spi_clocks_disable();
}

static irqreturn_t cfspi_irq(int irq, void *arg)
{
	/* Report SS trigger. */
	bool assert = !gpio_get_value(GPIO_MODEM_SPI_SS);
	if (assert)
		wake_lock(&cfspi.wake_lock_spi_ss);
	cfspi.sdev.ifc->ss_cb(assert, cfspi.sdev.ifc);
	if (!assert)
		wake_unlock(&cfspi.wake_lock_spi_ss);

	return IRQ_HANDLED;
}

static void cfspi_dma_cb(int lch, u16 ch_status, void *data)
{
	int stat;

	/* Check channel status. */
	if (ch_status != OMAP_DMA_BLOCK_IRQ)
		printk(KERN_ERR "ch_status = 0x%x\n", ch_status);

	/* Signal completion */
	if (data == &cfspi.tx_chnl)
		complete(&cfspi.dma_tx_completion);
	else
		complete(&cfspi.dma_rx_completion);

	/* Check if all transfers are completed - this makes the first call (TX) exit here */
	if (!down_trylock(&cfspi.sem)) {
		return;
	}

	/* Inform the SPI interface that the transfer is done, so sig_xfer can be called to finish up */
	cfspi.sdev.ifc->xfer_done_cb(cfspi.sdev.ifc);

	/* Check interrupts. */
	stat = __raw_readl(cfspi.io + OMAP2_MCSPI_IRQSTATUS_OFS);

	if (stat &
	    (OMAP2_MCSPI_IRQSTATUS_TX0UFLOW |
	     OMAP2_MCSPI_IRQSTATUS_RX0FULL | OMAP2_MCSPI_IRQSTATUS_RX0OFLOW)) {

		printk(KERN_WARNING "omap2_cfspi_xfer: IRQ stat: %x.\n", stat);

		/* Flush the data. */
		while (stat & OMAP2_MCSPI_IRQSTATUS_RX0FULL) {
			__u32 dummy;
			dummy = __raw_readl(cfspi.io + OMAP2_MCSPI2_RX0_ADR);

			printk(KERN_INFO "omap2_cfspi: data left: 0x%x.\n",
			       dummy);

			stat =
			    __raw_readl(cfspi.io + OMAP2_MCSPI_IRQSTATUS_OFS);
		}

		/* clear IRQs. */
		__raw_writel(stat, cfspi.io + OMAP2_MCSPI_IRQSTATUS_OFS);
	}
}

static int cfspi_init_xfer(struct cfspi_xfer *xfer, struct cfspi_dev *dev)
{
	/* Right now we need to transfer the max length in both directions. */
	if (xfer->tx_dma_len > xfer->rx_dma_len) {
		cfspi.dma_len = xfer->tx_dma_len;
	} else {
		cfspi.dma_len = xfer->rx_dma_len;
	}

	/* Set up and start dma transfers. */
	cfspi.tx_par.src_start = xfer->pa_tx;
	cfspi.tx_par.elem_count = cfspi.dma_len / SIZE_OF_WL;
	cfspi.rx_par.dst_start = xfer->pa_rx;
	cfspi.rx_par.elem_count = cfspi.dma_len / SIZE_OF_WL;
	omap_set_dma_params(cfspi.tx_chnl, &cfspi.tx_par);
	omap_set_dma_params(cfspi.rx_chnl, &cfspi.rx_par);

	up(&cfspi.sem);
	omap_start_dma(cfspi.tx_chnl);
	omap_start_dma(cfspi.rx_chnl);

	return 0;
}

void cfspi_sig_xfer(bool xfer, struct cfspi_dev *dev)
{
	if (xfer == true) {
		/* Remain active for duration of transfer */
		wake_lock(&cfspi.wake_lock_spi_int);
		/* Enable SPI */
		cfspi_spi_enable();
		/* Assert SPI_INT to signal start of transfer */
		gpio_set_value(GPIO_MODEM_SPI_INT, SPI_INT_ACTIVE);
	} else {
		/* Make sure transfer finished */
		wait_for_completion(&cfspi.dma_tx_completion);
		wait_for_completion(&cfspi.dma_rx_completion);
		/* Another try... */
		omap_stop_dma(cfspi.tx_chnl);
		omap_stop_dma(cfspi.rx_chnl);
		/* De-assert SPI_INT to signal end of transfer */
		gpio_set_value(GPIO_MODEM_SPI_INT, SPI_INT_INACTIVE);
		/* Disable SPI */
		cfspi_spi_disable();
		/* Transfer ended from our PoV */
		wake_unlock(&cfspi.wake_lock_spi_int);
	}
}

static int __init cfspi_inithw(void)
{
	int res;

	/* Configure GPIOs */
	res = gpio_request(GPIO_MODEM_SPI_SS, "cfspi_ss");
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't get gpio: %d.\n",
		       res, GPIO_MODEM_SPI_SS);
		goto err_request_gpio_ss;
	}

	/* Configure GPIO_MODEM_SPI_SS as input. */
	gpio_direction_input(GPIO_MODEM_SPI_SS);
	cfspi.irq = OMAP_GPIO_IRQ(GPIO_MODEM_SPI_SS);
	res = request_irq(cfspi.irq, cfspi_irq, SS_CHANGED, "cfspi", NULL);
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't get irq for gpio: %d.\n",
		       res, GPIO_MODEM_SPI_SS);
		goto err_request_irq_ss;
	}

	/* Make SS wake capable */
	res = enable_irq_wake(cfspi.irq);
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't make irq wake capable: %d.\n",
		       res, GPIO_MODEM_SPI_SS);
		goto err_request_irq_wken_ss;
	}

	res = gpio_request(GPIO_MODEM_SPI_INT, "cfspi_ss");
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't get gpio: %d.\n",
		       res, GPIO_MODEM_SPI_INT);
		goto err_request_gpio_spi_int;
	}
	/* Configure GPIO_MODEM_SPI_INT as output (inactive). */
	gpio_direction_output(GPIO_MODEM_SPI_INT, SPI_INT_INACTIVE);

	/* Initialize wake locks. */
	wake_lock_init(&cfspi.wake_lock_spi_int, WAKE_LOCK_SUSPEND,
			"caif-spi-int");
	wake_lock_init(&cfspi.wake_lock_spi_ss, WAKE_LOCK_SUSPEND,
			"caif-spi-ss");

	/* Get DMA resources. */
	res =
	    omap_request_dma(OMAP24XX_DMA_SPI2_TX0,
			     "cfspi_tx", cfspi_dma_cb, &cfspi.tx_chnl, &cfspi.tx_chnl);
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't get tx dma.\n", res);
		goto err_request_dma_tx;
	}
	res =
	    omap_request_dma(OMAP24XX_DMA_SPI2_RX0,
			     "cfspi_rx", cfspi_dma_cb, &cfspi.rx_chnl, &cfspi.rx_chnl);
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't get rx dma.\n", res);
		goto err_request_dma_rx;
	}

	/* Setup TX DMA configuration. */
	cfspi.tx_par.data_type = OMAP_DMA_DATA_TYPE_S16;
	cfspi.tx_par.frame_count = 1;
	cfspi.tx_par.elem_count = 1;
	cfspi.tx_par.src_amode = OMAP_DMA_AMODE_POST_INC;
	cfspi.tx_par.dst_amode = OMAP_DMA_AMODE_CONSTANT;
	cfspi.tx_par.dst_start = OMAP2_MCSPI2_TX0_ADR;
	cfspi.tx_par.trigger = OMAP24XX_DMA_SPI2_TX0;
	cfspi.tx_par.sync_mode = OMAP_DMA_SYNC_ELEMENT;
	cfspi.tx_par.src_or_dst_synch = 0;	/* Destination synchronized. */
	cfspi.tx_par.write_prio = DMA_CH_PRIO_HIGH;
	cfspi.tx_par.read_prio = DMA_CH_PRIO_HIGH;

	/* Setup RX DMA configuration. */
	cfspi.rx_par.data_type = OMAP_DMA_DATA_TYPE_S16;
	cfspi.rx_par.frame_count = 1;
	cfspi.rx_par.elem_count = 1;
	cfspi.rx_par.src_amode = OMAP_DMA_AMODE_CONSTANT;
	cfspi.rx_par.src_start = OMAP2_MCSPI2_RX0_ADR;
	cfspi.rx_par.dst_amode = OMAP_DMA_AMODE_POST_INC;
	cfspi.rx_par.trigger = OMAP24XX_DMA_SPI2_RX0;
	cfspi.rx_par.sync_mode = OMAP_DMA_SYNC_ELEMENT;
	cfspi.rx_par.src_or_dst_synch = 1;	/* Source synchronized. */
	cfspi.rx_par.write_prio = DMA_CH_PRIO_LOW;
	cfspi.rx_par.read_prio = DMA_CH_PRIO_LOW;

	/* Get SPI interface and functional clocks. */
	/* The kernel has to be rebuilt to use mcspi2_xxx default is mcspi_xxx. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	cfspi.ick = clk_get(&cfspi.pdev.dev, "ick");
#else
	cfspi.ick = clk_get(&cfspi.pdev.dev, "mcspi2_ick");
#endif
	if (!cfspi.ick) {
		printk(KERN_ERR "omap2_cfspi: failed to get spi ick.\n");
		res = -ENODEV;
		goto err_get_clk_ick;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	cfspi.fck = clk_get(&cfspi.pdev.dev, "fck");
#else
	cfspi.fck = clk_get(&cfspi.pdev.dev, "mcspi2_fck");
#endif
	if (!cfspi.fck) {
		printk(KERN_ERR "omap2_cfspi: failed to get spi fck.\n");
		res = -ENODEV;
		goto err_get_clk_fck;
	}

	/* Map the SPI module. */
	cfspi.io = ioremap(OMAP2_MCSPI2_BASE_ADR, 256);
	if (!cfspi.io) {
		printk(KERN_ERR "omap2_cfspi: failed to iomap spi regs.\n");
		res = -ENODEV;
		goto err_ioremap_spi;
	}

	/* Setup blockers for data transfer completion */
	init_completion(&cfspi.dma_rx_completion);
	init_completion(&cfspi.dma_tx_completion);

	/* Setup SPI configuration */
	context.sysconfig = OMAP2_MCSPI_SYSCONFIG_AUTOIDLE |
			OMAP2_MCSPI_SYSCONFIG_ENAWAKEUP |
			OMAP2_MCSPI_SYSCONFIG_SMARTIDLE;

	context.wakeupenable = OMAP2_MCSPI_WAKEUPENABLE_WKEN;

	/* XFERLEVEL: Minimum transfer size in both directions is 4 bytes. */
	context.xfer_level = OMAP2_MCSPI_XFERLEVEL_AEL(2) |
		     OMAP2_MCSPI_XFERLEVEL_AFL(2);

	/* We will assign all resources to the TX path. */
	/* CHCONF: Reception on SPI_SIMO, transmission on SPI_SOMI, use DMA both for TX and RX, 16 bit word length, SPI_CS active low. */
	context.channel_conf = OMAP2_MCSPI_CHCONF_IS | OMAP2_MCSPI_CHCONF_DPE1 |
		     OMAP2_MCSPI_CHCONF_DMAW | OMAP2_MCSPI_CHCONF_DMAR |
		     OMAP2_MCSPI_CHCONF_FFEW | OMAP2_MCSPI_CHCONF_FFER |
		     OMAP2_MCSPI_CHCONF_WL(16) | OMAP2_MCSPI_CHCONF_EPOL;

	/* Start SPI */
	res = cfspi_spi_enable();
	if (res < 0) {
		printk(KERN_ERR
		       "omap2_cfspi: err: %d, can't enable clocks.\n", res);
		goto err_clk_enable;
	}

	/* Stop SPI */
	cfspi_spi_disable();

	return res;

 err_clk_enable:
	iounmap(cfspi.io);
 err_ioremap_spi:
	clk_put(cfspi.fck);
 err_get_clk_fck:
	clk_put(cfspi.ick);
 err_get_clk_ick:
	omap_free_dma(cfspi.rx_chnl);
 err_request_dma_rx:
	omap_free_dma(cfspi.tx_chnl);
 err_request_dma_tx:
	gpio_free(GPIO_MODEM_SPI_INT);
 err_request_gpio_spi_int:
	disable_irq_wake(OMAP_GPIO_IRQ(GPIO_MODEM_SPI_SS));
 err_request_irq_wken_ss:
	free_irq(OMAP_GPIO_IRQ(GPIO_MODEM_SPI_SS), NULL);
 err_request_irq_ss:
	gpio_free(GPIO_MODEM_SPI_SS);
 err_request_gpio_ss:

	return res;
}

static int __init omap_cfspi_init(void)
{
	int res = 0;

	/* Initialize simulated slave device. */
	cfspi.sdev.init_xfer = cfspi_init_xfer;
	cfspi.sdev.sig_xfer = cfspi_sig_xfer;
	cfspi.sdev.clk_mhz = 13;
	cfspi.sdev.priv = &cfspi;
	cfspi.sdev.name = "spi_omap";

	/* Initialize platform device. */
	cfspi.pdev.name = "cfspi_sspi";
	cfspi.pdev.dev.platform_data = &cfspi.sdev;

	/* Initialize semaphore. */
	sema_init(&cfspi.sem, 0);

	/* We need to register to get the clocks. Bind to dummy platform
	 * device or use spin_lock. */

	/* Register platform device. */
	res = platform_device_register(&cfspi.pdev);
	if (res) {
		printk(KERN_ERR "omap_cfspi: failed to register.\n");
		res = -ENODEV;
		goto err_register_platform_device;
	}

	/* Call HW setup. */
	res = cfspi_inithw();
	if (res) {
		printk(KERN_ERR "omap_cfspi: failed to init HW.\n");
		res = -ENODEV;
		goto err_initializing_hw;
	}

	/* HW setup done release spin lock. */

	return res;

 err_initializing_hw:
	platform_device_unregister(&cfspi.pdev);
 err_register_platform_device:

	return res;
}

static void __exit omap_cfspi_exit(void)
{
	iounmap(cfspi.io);
	clk_put(cfspi.fck);
	clk_put(cfspi.ick);
	omap_free_dma(cfspi.rx_chnl);
	omap_free_dma(cfspi.tx_chnl);
	gpio_free(GPIO_MODEM_SPI_INT);
	disable_irq_wake(OMAP_GPIO_IRQ(GPIO_MODEM_SPI_SS));
	free_irq(cfspi.irq, NULL);
	gpio_free(GPIO_MODEM_SPI_SS);

	/* Unregister platform device. */
	platform_device_unregister(&cfspi.pdev);
}

module_init(omap_cfspi_init);
module_exit(omap_cfspi_exit);
