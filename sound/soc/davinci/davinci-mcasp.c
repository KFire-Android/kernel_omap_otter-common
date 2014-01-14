/*
 * ALSA SoC McASP Audio Layer for TI DAVINCI processor
 *
 * Multi-channel Audio Serial Port Driver
 *
 * Author: Nirmal Pandey <n-pandey@ti.com>,
 *         Suresh Rajashekara <suresh.r@ti.com>
 *         Steve Chen <schen@.mvista.com>
 *
 * Copyright:   (C) 2009 MontaVista Software, Inc., <source@mvista.com>
 * Copyright:   (C) 2009  Texas Instruments, India
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/lcm.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "davinci-pcm.h"
#include "davinci-mcasp.h"
#include "../omap/omap-pcm.h"

/*
 * McASP register definitions
 */
#define DAVINCI_MCASP_PID_REG		0x00
#define DAVINCI_MCASP_PWREMUMGT_REG	0x04

#define DAVINCI_MCASP_PFUNC_REG		0x10
#define DAVINCI_MCASP_PDIR_REG		0x14
#define DAVINCI_MCASP_PDOUT_REG		0x18
#define DAVINCI_MCASP_PDSET_REG		0x1c

#define DAVINCI_MCASP_PDCLR_REG		0x20

#define DAVINCI_MCASP_TLGC_REG		0x30
#define DAVINCI_MCASP_TLMR_REG		0x34

#define DAVINCI_MCASP_GBLCTL_REG	0x44
#define DAVINCI_MCASP_AMUTE_REG		0x48
#define DAVINCI_MCASP_LBCTL_REG		0x4c

#define DAVINCI_MCASP_TXDITCTL_REG	0x50

#define DAVINCI_MCASP_GBLCTLR_REG	0x60
#define DAVINCI_MCASP_RXMASK_REG	0x64
#define DAVINCI_MCASP_RXFMT_REG		0x68
#define DAVINCI_MCASP_RXFMCTL_REG	0x6c

#define DAVINCI_MCASP_ACLKRCTL_REG	0x70
#define DAVINCI_MCASP_AHCLKRCTL_REG	0x74
#define DAVINCI_MCASP_RXTDM_REG		0x78
#define DAVINCI_MCASP_EVTCTLR_REG	0x7c

#define DAVINCI_MCASP_RXSTAT_REG	0x80
#define DAVINCI_MCASP_RXTDMSLOT_REG	0x84
#define DAVINCI_MCASP_RXCLKCHK_REG	0x88
#define DAVINCI_MCASP_REVTCTL_REG	0x8c

#define DAVINCI_MCASP_GBLCTLX_REG	0xa0
#define DAVINCI_MCASP_TXMASK_REG	0xa4
#define DAVINCI_MCASP_TXFMT_REG		0xa8
#define DAVINCI_MCASP_TXFMCTL_REG	0xac

#define DAVINCI_MCASP_ACLKXCTL_REG	0xb0
#define DAVINCI_MCASP_AHCLKXCTL_REG	0xb4
#define DAVINCI_MCASP_TXTDM_REG		0xb8
#define DAVINCI_MCASP_EVTCTLX_REG	0xbc

#define DAVINCI_MCASP_TXSTAT_REG	0xc0
#define DAVINCI_MCASP_TXTDMSLOT_REG	0xc4
#define DAVINCI_MCASP_TXCLKCHK_REG	0xc8
#define DAVINCI_MCASP_XEVTCTL_REG	0xcc

/* Left(even TDM Slot) Channel Status Register File */
#define DAVINCI_MCASP_DITCSRA_REG	0x100
/* Right(odd TDM slot) Channel Status Register File */
#define DAVINCI_MCASP_DITCSRB_REG	0x118
/* Left(even TDM slot) User Data Register File */
#define DAVINCI_MCASP_DITUDRA_REG	0x130
/* Right(odd TDM Slot) User Data Register File */
#define DAVINCI_MCASP_DITUDRB_REG	0x148

/* Serializer n Control Register */
#define DAVINCI_MCASP_XRSRCTL_BASE_REG	0x180
#define DAVINCI_MCASP_XRSRCTL_REG(n)	(DAVINCI_MCASP_XRSRCTL_BASE_REG + \
						(n << 2))

/* Transmit Buffer for Serializer n */
#define DAVINCI_MCASP_TXBUF_REG		0x200
/* Receive Buffer for Serializer n */
#define DAVINCI_MCASP_RXBUF_REG		0x284

/* McASP FIFO Registers */
#define DAVINCI_MCASP_WFIFOCTL		(0x1010)
#define DAVINCI_MCASP_WFIFOSTS		(0x1014)
#define DAVINCI_MCASP_RFIFOCTL		(0x1018)
#define DAVINCI_MCASP_RFIFOSTS		(0x101C)
#define MCASP_VER3_WFIFOCTL		(0x1000)
#define MCASP_VER3_WFIFOSTS		(0x1004)
#define MCASP_VER3_RFIFOCTL		(0x1008)
#define MCASP_VER3_RFIFOSTS		(0x100C)

/*
 * DAVINCI_MCASP_PWREMUMGT_REG - Power Down and Emulation Management
 *     Register Bits
 */
#define MCASP_FREE	BIT(0)
#define MCASP_SOFT	BIT(1)

/*
 * DAVINCI_MCASP_PFUNC_REG - Pin Function / GPIO Enable Register Bits
 */
#define AXR(n)		(1<<n)
#define PFUNC_AMUTE	BIT(25)
#define ACLKX		BIT(26)
#define AHCLKX		BIT(27)
#define AFSX		BIT(28)
#define ACLKR		BIT(29)
#define AHCLKR		BIT(30)
#define AFSR		BIT(31)

/*
 * DAVINCI_MCASP_PDIR_REG - Pin Direction Register Bits
 */
#define AXR(n)		(1<<n)
#define PDIR_AMUTE	BIT(25)
#define ACLKX		BIT(26)
#define AHCLKX		BIT(27)
#define AFSX		BIT(28)
#define ACLKR		BIT(29)
#define AHCLKR		BIT(30)
#define AFSR		BIT(31)

/*
 * DAVINCI_MCASP_TXDITCTL_REG - Transmit DIT Control Register Bits
 */
#define DITEN	BIT(0)	/* Transmit DIT mode enable/disable */
#define VA	BIT(2)
#define VB	BIT(3)

/*
 * DAVINCI_MCASP_TXFMT_REG - Transmit Bitstream Format Register Bits
 */
#define TXROT(val)	(val)
#define TXSEL		BIT(3)
#define TXSSZ(val)	(val<<4)
#define TXPBIT(val)	(val<<8)
#define TXPAD(val)	(val<<13)
#define TXORD		BIT(15)
#define FSXDLY(val)	(val<<16)

/*
 * DAVINCI_MCASP_RXFMT_REG - Receive Bitstream Format Register Bits
 */
#define RXROT(val)	(val)
#define RXSEL		BIT(3)
#define RXSSZ(val)	(val<<4)
#define RXPBIT(val)	(val<<8)
#define RXPAD(val)	(val<<13)
#define RXORD		BIT(15)
#define FSRDLY(val)	(val<<16)

/*
 * DAVINCI_MCASP_TXFMCTL_REG -  Transmit Frame Control Register Bits
 */
#define FSXPOL		BIT(0)
#define AFSXE		BIT(1)
#define FSXDUR		BIT(4)
#define FSXMOD(val)	(val<<7)

/*
 * DAVINCI_MCASP_RXFMCTL_REG - Receive Frame Control Register Bits
 */
#define FSRPOL		BIT(0)
#define AFSRE		BIT(1)
#define FSRDUR		BIT(4)
#define FSRMOD(val)	(val<<7)

/*
 * DAVINCI_MCASP_ACLKXCTL_REG - Transmit Clock Control Register Bits
 */
#define ACLKXDIV(val)	(val)
#define ACLKXE		BIT(5)
#define TX_ASYNC	BIT(6)
#define ACLKXPOL	BIT(7)
#define ACLKXDIV_MASK	0x1f

/*
 * DAVINCI_MCASP_ACLKRCTL_REG Receive Clock Control Register Bits
 */
#define ACLKRDIV(val)	(val)
#define ACLKRE		BIT(5)
#define RX_ASYNC	BIT(6)
#define ACLKRPOL	BIT(7)
#define ACLKRDIV_MASK	0x1f

/*
 * DAVINCI_MCASP_AHCLKXCTL_REG - High Frequency Transmit Clock Control
 *     Register Bits
 */
#define AHCLKXDIV(val)	(val)
#define AHCLKXPOL	BIT(14)
#define AHCLKXE		BIT(15)
#define AHCLKXDIV_MASK	0xfff

/*
 * DAVINCI_MCASP_AHCLKRCTL_REG - High Frequency Receive Clock Control
 *     Register Bits
 */
#define AHCLKRDIV(val)	(val)
#define AHCLKRPOL	BIT(14)
#define AHCLKRE		BIT(15)
#define AHCLKRDIV_MASK	0xfff

/*
 * DAVINCI_MCASP_XRSRCTL_BASE_REG -  Serializer Control Register Bits
 */
#define MODE(val)	(val)
#define DISMOD(val)	(val << 2)
#define TXSTATE		BIT(4)
#define RXSTATE		BIT(5)
#define SRMOD_MASK	3
#define SRMOD_INACTIVE	0

/*
 * DAVINCI_MCASP_LBCTL_REG - Loop Back Control Register Bits
 */
#define LBEN		BIT(0)
#define LBORD		BIT(1)
#define LBGENMODE(val)	(val<<2)

/*
 * DAVINCI_MCASP_TXTDMSLOT_REG - Transmit TDM Slot Register configuration
 */
#define TXTDMS(n)	(1<<n)

/*
 * DAVINCI_MCASP_RXTDMSLOT_REG - Receive TDM Slot Register configuration
 */
#define RXTDMS(n)	(1<<n)

/*
 * DAVINCI_MCASP_GBLCTL_REG -  Global Control Register Bits
 */
#define RXCLKRST	BIT(0)	/* Receiver Clock Divider Reset */
#define RXHCLKRST	BIT(1)	/* Receiver High Frequency Clock Divider */
#define RXSERCLR	BIT(2)	/* Receiver Serializer Clear */
#define RXSMRST		BIT(3)	/* Receiver State Machine Reset */
#define RXFSRST		BIT(4)	/* Frame Sync Generator Reset */
#define TXCLKRST	BIT(8)	/* Transmitter Clock Divider Reset */
#define TXHCLKRST	BIT(9)	/* Transmitter High Frequency Clock Divider*/
#define TXSERCLR	BIT(10)	/* Transmit Serializer Clear */
#define TXSMRST		BIT(11)	/* Transmitter State Machine Reset */
#define TXFSRST		BIT(12)	/* Frame Sync Generator Reset */

/*
 * DAVINCI_MCASP_AMUTE_REG -  Mute Control Register Bits
 */
#define MUTENA(val)	(val)
#define MUTEINPOL	BIT(2)
#define MUTEINENA	BIT(3)
#define MUTEIN		BIT(4)
#define MUTER		BIT(5)
#define MUTEX		BIT(6)
#define MUTEFSR		BIT(7)
#define MUTEFSX		BIT(8)
#define MUTEBADCLKR	BIT(9)
#define MUTEBADCLKX	BIT(10)
#define MUTERXDMAERR	BIT(11)
#define MUTETXDMAERR	BIT(12)

/*
 * DAVINCI_MCASP_REVTCTL_REG - Receiver DMA Event Control Register bits
 */
#define RXDATADMADIS	BIT(0)

/*
 * DAVINCI_MCASP_XEVTCTL_REG - Transmitter DMA Event Control Register bits
 */
#define TXDATADMADIS	BIT(0)

/*
 * DAVINCI_MCASP_EVTCTLR_REG - Receiver Interrupt Control Register Bits
 */
#define ROVRN		BIT(0)

/*
 * DAVINCI_MCASP_EVTCTLX_REG - Transmitter Interrupt Control Register Bits
 */
#define XUNDRN		BIT(0)

/*
 * DAVINCI_MCASP_W[R]FIFOCTL - Write/Read FIFO Control Register bits
 */
#define FIFO_ENABLE	BIT(16)
#define NUMEVT_MASK	(0xFF << 8)
#define NUMDMA_MASK	(0xFF)

#define DAVINCI_MCASP_NUM_SERIALIZER	16

/*
 * Timeout value of 1 ms was chosen experimentally to account for the initial
 * latency to have the first audio sample transferred to AFIFO by DMA
 */
#define MCASP_FIFO_FILL_TIMEOUT		1000

static inline void mcasp_set_bits(void __iomem *reg, u32 val)
{
	__raw_writel(__raw_readl(reg) | val, reg);
}

static inline void mcasp_clr_bits(void __iomem *reg, u32 val)
{
	__raw_writel((__raw_readl(reg) & ~(val)), reg);
}

static inline void mcasp_mod_bits(void __iomem *reg, u32 val, u32 mask)
{
	__raw_writel((__raw_readl(reg) & ~mask) | val, reg);
}

static inline void mcasp_set_reg(void __iomem *reg, u32 val)
{
	__raw_writel(val, reg);
}

static inline u32 mcasp_get_reg(void __iomem *reg)
{
	return (unsigned int)__raw_readl(reg);
}

static inline void mcasp_set_ctl_reg(void __iomem *regs, u32 val)
{
	int i = 0;

	mcasp_set_bits(regs, val);

	/* programming GBLCTL needs to read back from GBLCTL and verfiy */
	/* loop count is to avoid the lock-up */
	for (i = 0; i < 1000; i++) {
		if ((mcasp_get_reg(regs) & val) == val)
			break;
	}

	if (i == 1000 && ((mcasp_get_reg(regs) & val) != val))
		printk(KERN_ERR "GBLCTL write error\n");
}

static void mcasp_start_rx(struct davinci_audio_dev *dev)
{
	u32 rxfmctl = mcasp_get_reg(dev->base + DAVINCI_MCASP_RXFMCTL_REG);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXHCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXCLKRST);

	/*
	 * Enable TX FSG when McASP is in sync mode and is master since the
	 * TX section provides FSYNC for RX too. TX clk dividers are also
	 * enabled for cases where ACLKR pin is not connected (typically
	 * done when AFSR is not used either)
	 */
	if (rxfmctl & AFSRE) {
		mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG,
				  TXHCLKRST);
		mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG,
				  TXCLKRST);
	}

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXSERCLR);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXBUF_REG, 0);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXSMRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXFSRST);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXBUF_REG, 0);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXSMRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXFSRST);

	if (rxfmctl & AFSRE)
		mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG,
				  TXFSRST);

	/* enable IRQ sources */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_EVTCTLR_REG, ROVRN);
}

static void mcasp_start_tx(struct davinci_audio_dev *dev)
{
	u8 offset = 0, i;
	u32 cnt;

	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXMASK_REG, dev->bit_mask);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXHCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXSERCLR);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXBUF_REG, 0);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXSMRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXFSRST);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXBUF_REG, 0);
	for (i = 0; i < dev->num_serializer; i++) {
		if (dev->serial_dir[i] == TX_MODE) {
			offset = i;
			break;
		}
	}

	/* wait for TX ready */
	cnt = 0;
	while (!(mcasp_get_reg(dev->base + DAVINCI_MCASP_XRSRCTL_REG(offset)) &
		 TXSTATE) && (cnt < 100000))
		cnt++;

	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXBUF_REG, 0);

	/* enable IRQ sources */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_EVTCTLX_REG, XUNDRN);
}

static int davinci_mcasp_start(struct davinci_audio_dev *dev, int stream)
{
	int i = 0;
	u32 val;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (dev->txnumevt) {	/* enable FIFO */
			switch (dev->version) {
			case MCASP_VERSION_3:
			case MCASP_VERSION_4:
				mcasp_clr_bits(dev->base + MCASP_VER3_WFIFOCTL,
								FIFO_ENABLE);
				mcasp_set_bits(dev->base + MCASP_VER3_WFIFOCTL,
								FIFO_ENABLE);
				break;
			default:
				mcasp_clr_bits(dev->base +
					DAVINCI_MCASP_WFIFOCTL,	FIFO_ENABLE);
				mcasp_set_bits(dev->base +
					DAVINCI_MCASP_WFIFOCTL,	FIFO_ENABLE);
			}

			/*
			 * Wait until DMA has loaded at least one sample into
			 * AFIFO to ensure XRUN is not immediately hit
			 * Implementation has to use udelay since it's executed
			 * in atomic context (trigger() callback)
			 */
			while (++i) {
				val = mcasp_get_reg(dev->base +
						    MCASP_VER3_WFIFOSTS);
				if (val > 0)
					break;

				if (i > MCASP_FIFO_FILL_TIMEOUT)
					return -ETIMEDOUT;

				udelay(1);
			}
		}
		mcasp_start_tx(dev);
	} else {
		if (dev->rxnumevt) {	/* enable FIFO */
			switch (dev->version) {
			case MCASP_VERSION_3:
			case MCASP_VERSION_4:
				mcasp_clr_bits(dev->base + MCASP_VER3_RFIFOCTL,
								FIFO_ENABLE);
				mcasp_set_bits(dev->base + MCASP_VER3_RFIFOCTL,
								FIFO_ENABLE);
				break;
			default:
				mcasp_clr_bits(dev->base +
					DAVINCI_MCASP_RFIFOCTL,	FIFO_ENABLE);
				mcasp_set_bits(dev->base +
					DAVINCI_MCASP_RFIFOCTL,	FIFO_ENABLE);
			}
		}
		mcasp_start_rx(dev);
	}

	return 0;
}

static void mcasp_stop_rx(struct davinci_audio_dev *dev)
{
	u32 gblctl = mcasp_get_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG);
	u32 rxfmctl = mcasp_get_reg(dev->base + DAVINCI_MCASP_RXFMCTL_REG);

	/* disable IRQ sources */
	mcasp_clr_bits(dev->base + DAVINCI_MCASP_EVTCTLR_REG, ROVRN);

	/* Disable TX FSG if RX was the only active user */
	if ((rxfmctl & AFSRE) && !(gblctl & TXSMRST))
		mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, 0);

	mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, 0);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXSTAT_REG, 0xFFFFFFFF);
}

static void mcasp_stop_tx(struct davinci_audio_dev *dev)
{
	u32 gblctl = mcasp_get_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG);
	u32 rxfmctl = mcasp_get_reg(dev->base + DAVINCI_MCASP_RXFMCTL_REG);
	u32 val = 0;

	/* disable IRQ sources */
	mcasp_clr_bits(dev->base + DAVINCI_MCASP_EVTCTLX_REG, XUNDRN);

	/* Keep FSG active until RX section is stopped too */
	if ((rxfmctl & AFSRE) && (gblctl & RXSMRST))
		val =  TXHCLKRST | TXCLKRST | TXFSRST;

	/*
	 * Reset of the state machine, serializer and frame sync generator
	 * can occur in the middle of a slot which can cause discontinuities
	 * that may lead to glitches. It can be prevented muting the transmit
	 * data by masking out all its bits.
	 * A delay is required to ensure at least one slot uses the new TXMASK,
	 * the worst case (longest slot) is for 8kHz, mono (125 us).
	 * Implementation has to use udelay since it's executed in atomic
	 * context (trigger() callback).
	 */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXMASK_REG, 0);
	udelay(125);

	mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, val);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXSTAT_REG, 0xFFFFFFFF);
}

static void davinci_mcasp_stop(struct davinci_audio_dev *dev, int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (dev->txnumevt) {	/* disable FIFO */
			switch (dev->version) {
			case MCASP_VERSION_3:
			case MCASP_VERSION_4:
				mcasp_clr_bits(dev->base + MCASP_VER3_WFIFOCTL,
								FIFO_ENABLE);
				break;
			default:
				mcasp_clr_bits(dev->base +
					DAVINCI_MCASP_WFIFOCTL,	FIFO_ENABLE);
			}
		}
		mcasp_stop_tx(dev);
	} else {
		if (dev->rxnumevt) {	/* disable FIFO */
			switch (dev->version) {
			case MCASP_VERSION_3:
			case MCASP_VERSION_4:
				mcasp_clr_bits(dev->base + MCASP_VER3_RFIFOCTL,
								FIFO_ENABLE);
			break;

			default:
				mcasp_clr_bits(dev->base +
					DAVINCI_MCASP_RFIFOCTL,	FIFO_ENABLE);
			}
		}
		mcasp_stop_rx(dev);
	}
}

static irqreturn_t davinci_mcasp_tx_irq_handler(int irq, void *data)
{
	struct davinci_audio_dev *dev = (struct davinci_audio_dev *)data;
	struct snd_pcm_substream *substream;
	u32 stat;

	stat = mcasp_get_reg(dev->base + DAVINCI_MCASP_TXSTAT_REG);
	if (stat & XUNDRN) {
		dev_dbg(dev->dev, "Transmit buffer underflow\n");
		substream = dev->substreams[SNDRV_PCM_STREAM_PLAYBACK];
		if (substream) {
			snd_pcm_stream_lock_irq(substream);
			if (snd_pcm_running(substream))
				snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
			snd_pcm_stream_unlock_irq(substream);
		}
	}

	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXSTAT_REG, stat);

	return IRQ_HANDLED;
}

static irqreturn_t davinci_mcasp_rx_irq_handler(int irq, void *data)
{
	struct davinci_audio_dev *dev = (struct davinci_audio_dev *)data;
	struct snd_pcm_substream *substream;
	u32 stat;

	stat = mcasp_get_reg(dev->base + DAVINCI_MCASP_RXSTAT_REG);
	if (stat & ROVRN) {
		dev_dbg(dev->dev, "Receive buffer overflow\n");
		substream = dev->substreams[SNDRV_PCM_STREAM_CAPTURE];
		if (substream) {
			snd_pcm_stream_lock_irq(substream);
			if (snd_pcm_running(substream))
				snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
			snd_pcm_stream_unlock_irq(substream);
		}
	}

	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXSTAT_REG, stat);

	return IRQ_HANDLED;
}

static int davinci_mcasp_set_dai_fmt(struct snd_soc_dai *cpu_dai,
					 unsigned int fmt)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	void __iomem *base = dev->base;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_B:
	case SND_SOC_DAIFMT_AC97:
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG, FSXDUR);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG, FSRDUR);
		break;
	default:
		/* configure a full-word SYNC pulse (LRCLK) */
		mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG, FSXDUR);
		mcasp_set_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG, FSRDUR);

		/* make 1st data bit occur one ACLK cycle after the frame sync */
		mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMT_REG, FSXDLY(1));
		mcasp_set_bits(dev->base + DAVINCI_MCASP_RXFMT_REG, FSRDLY(1));
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* codec is clock and frame slave */
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);

		mcasp_set_bits(base + DAVINCI_MCASP_PDIR_REG,
				ACLKX | ACLKR);
		mcasp_set_bits(base + DAVINCI_MCASP_PDIR_REG,
				AFSX | AFSR);
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		/* codec is clock master and frame slave */
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);

		mcasp_clr_bits(base + DAVINCI_MCASP_PDIR_REG,
				ACLKX | ACLKR);
		mcasp_set_bits(base + DAVINCI_MCASP_PDIR_REG,
				AFSX | AFSR);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		/* codec is clock and frame master */
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);

		mcasp_clr_bits(base + DAVINCI_MCASP_PDIR_REG,
				ACLKX | AHCLKX | AFSX | ACLKR | AHCLKR | AFSR);
		break;

	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_NF:
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	case SND_SOC_DAIFMT_NB_IF:
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	case SND_SOC_DAIFMT_IB_IF:
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	case SND_SOC_DAIFMT_NB_NF:
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int davinci_mcasp_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(dai);

	switch (div_id) {
	case 0:		/* MCLK divider */
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_AHCLKXCTL_REG,
			       AHCLKXDIV(div - 1), AHCLKXDIV_MASK);
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_AHCLKRCTL_REG,
			       AHCLKRDIV(div - 1), AHCLKRDIV_MASK);
		break;

	case 1:		/* BCLK divider */
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG,
			       ACLKXDIV(div - 1), ACLKXDIV_MASK);
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_ACLKRCTL_REG,
			       ACLKRDIV(div - 1), ACLKRDIV_MASK);
		break;

	case 2:		/* BCLK/LRCLK ratio */
		dev->bclk_lrclk_ratio = div;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int davinci_mcasp_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				    unsigned int freq, int dir)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (dir == SND_SOC_CLOCK_OUT) {
		mcasp_set_bits(dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXE);
		mcasp_set_bits(dev->base + DAVINCI_MCASP_AHCLKRCTL_REG, AHCLKRE);
		mcasp_set_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AHCLKX);
	} else {
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXE);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_AHCLKRCTL_REG, AHCLKRE);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AHCLKX);
	}

	return 0;
}

static int davinci_config_channel_size(struct davinci_audio_dev *dev,
				       int word_length)
{
	u32 fmt;
	u32 tx_rotate = (word_length / 4) & 0x7;
	u32 rx_rotate;
	u32 slot_length;

	dev->bit_mask = (1ULL << word_length) - 1;

	/*
	 * if s BCLK-to-LRCLK ratio has been configured via the set_clkdiv()
	 * callback, take it into account here. That allows us to for example
	 * send 32 bits per channel to the codec, while only 16 of them carry
	 * audio payload.
	 * The clock ratio is given for a full period of data (for I2S format
	 * both left and right channels), so it has to be divided by number of
	 * tdm-slots (for I2S - divided by 2).
	 */
	if (dev->bclk_lrclk_ratio)
		slot_length = dev->bclk_lrclk_ratio / dev->tdm_slots;
	else
		slot_length = word_length;

	/* mapping of the XSSZ bit-field as described in the datasheet */
	fmt = (slot_length >> 1) - 1;

	rx_rotate = (slot_length - word_length) / 4;

	if (dev->op_mode != DAVINCI_MCASP_DIT_MODE) {
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMT_REG,
				RXSSZ(fmt), RXSSZ(0x0F));
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMT_REG,
				TXSSZ(fmt), TXSSZ(0x0F));
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMT_REG,
				TXROT(tx_rotate), TXROT(7));
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMT_REG,
				RXROT(rx_rotate), RXROT(7));
		mcasp_set_reg(dev->base + DAVINCI_MCASP_RXMASK_REG,
				dev->bit_mask);
	}

	return 0;
}

static int davinci_hw_common_param(struct davinci_audio_dev *dev, int stream,
				    int channels)
{
	int i;
	u8 tx_ser = 0;
	u8 rx_ser = 0;
	u8 ser;
	u8 slots = dev->tdm_slots;
	u8 max_active_serializers = (channels + slots - 1) / slots;
	/* Default configuration */
	if (dev->version != MCASP_VERSION_4)
		mcasp_set_bits(dev->base + DAVINCI_MCASP_PWREMUMGT_REG,
				MCASP_SOFT);

	/* All PINS as McASP */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_PFUNC_REG, 0x00000000);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mcasp_set_reg(dev->base + DAVINCI_MCASP_TXSTAT_REG, 0xFFFFFFFF);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_XEVTCTL_REG,
				TXDATADMADIS);
	} else {
		mcasp_set_reg(dev->base + DAVINCI_MCASP_RXSTAT_REG, 0xFFFFFFFF);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_REVTCTL_REG,
				RXDATADMADIS);
	}

	for (i = 0; i < dev->num_serializer; i++) {
		mcasp_set_bits(dev->base + DAVINCI_MCASP_XRSRCTL_REG(i),
					dev->serial_dir[i]);
		if (dev->serial_dir[i] == TX_MODE &&
					tx_ser < max_active_serializers) {
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_XRSRCTL_REG(i),
				       DISMOD(dev->tx_dismod), DISMOD(3));

			mcasp_set_bits(dev->base + DAVINCI_MCASP_PDIR_REG,
					AXR(i));
			tx_ser++;
		} else if (dev->serial_dir[i] == RX_MODE &&
					rx_ser < max_active_serializers) {
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_XRSRCTL_REG(i),
				       DISMOD(dev->rx_dismod), DISMOD(3));

			mcasp_clr_bits(dev->base + DAVINCI_MCASP_PDIR_REG,
					AXR(i));
			rx_ser++;
		} else {
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_XRSRCTL_REG(i),
					SRMOD_INACTIVE, SRMOD_MASK);
		}
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		ser = tx_ser;
	else
		ser = rx_ser;

	if (ser < max_active_serializers) {
		dev_warn(dev->dev, "stream has more channels (%d) than are "
			"enabled in mcasp (%d)\n", channels, ser * slots);
		return -EINVAL;
	}

	if (dev->txnumevt && stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (dev->txnumevt * tx_ser > 64)
			dev->txnumevt = 1;

		switch (dev->version) {
		case MCASP_VERSION_3:
		case MCASP_VERSION_4:
			mcasp_mod_bits(dev->base + MCASP_VER3_WFIFOCTL, tx_ser,
								NUMDMA_MASK);
			mcasp_mod_bits(dev->base + MCASP_VER3_WFIFOCTL,
				((dev->txnumevt * tx_ser) << 8), NUMEVT_MASK);
			break;
		default:
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_WFIFOCTL,
							tx_ser,	NUMDMA_MASK);
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_WFIFOCTL,
				((dev->txnumevt * tx_ser) << 8), NUMEVT_MASK);
		}
	}

	if (dev->rxnumevt && stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (dev->rxnumevt * rx_ser > 64)
			dev->rxnumevt = 1;
		switch (dev->version) {
		case MCASP_VERSION_3:
		case MCASP_VERSION_4:
			mcasp_mod_bits(dev->base + MCASP_VER3_RFIFOCTL, rx_ser,
								NUMDMA_MASK);
			mcasp_mod_bits(dev->base + MCASP_VER3_RFIFOCTL,
				((dev->rxnumevt * rx_ser) << 8), NUMEVT_MASK);
			break;
		default:
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_RFIFOCTL,
							rx_ser,	NUMDMA_MASK);
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_RFIFOCTL,
				((dev->rxnumevt * rx_ser) << 8), NUMEVT_MASK);
		}
	}

	return 0;
}

static int davinci_hw_param(struct davinci_audio_dev *dev, int stream,
			    int channels)
{
	int i, active_slots;
	int total_slots = dev->tdm_slots;
	int active_serializers;
	u32 mask = 0;
	u32 busel = 0;

	if ((total_slots < 2) || (total_slots > 32)) {
		dev_err(dev->dev, "tdm slot count %d not supported\n",
			total_slots);
		return -EINVAL;
	}

	/*
	 * If more than one serializer is needed, then use them with
	 * their specified tdm_slots count. Otherwise, one serializer
	 * can cope with the transaction using as many slots as channels
	 * in the stream, requires channels symmetry
	 */
	active_serializers = (channels + total_slots - 1) / total_slots;
	if (active_serializers == 1) {
		active_slots = channels;
		dev->channels = channels;
	} else {
		active_slots = total_slots;
	}

	for (i = 0; i < active_slots; i++)
		mask |= (1 << i);

	mcasp_clr_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG, TX_ASYNC);

	if (dev->version == MCASP_VERSION_4 && !dev->dat_port)
		busel = TXSEL;

	/* bit stream is MSB first  with no delay */
	/* DSP_B mode */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXTDM_REG, mask);
	mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMT_REG, busel | TXORD);
	mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG,
		       FSXMOD(total_slots), FSXMOD(0x1FF));

	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXTDM_REG, mask);
	mcasp_set_bits(dev->base + DAVINCI_MCASP_RXFMT_REG, busel | RXORD);
	mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG,
		       FSRMOD(total_slots), FSRMOD(0x1FF));

	return 0;
}

/* S/PDIF */
static int davinci_hw_dit_param(struct davinci_audio_dev *dev)
{
	/* Set the TX format : 24 bit right rotation, 32 bit slot, Pad 0
	   and LSB first */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMT_REG,
						TXROT(6) | TXSSZ(15));

	/* Set TX frame synch : DIT Mode, 1 bit width, internal, rising edge */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXFMCTL_REG,
						AFSXE | FSXMOD(0x180));

	/* Set the TX tdm : for all the slots */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXTDM_REG, 0xFFFFFFFF);

	/* Set the TX clock controls : div = 1 and internal */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG,
						ACLKXE | TX_ASYNC);

	mcasp_clr_bits(dev->base + DAVINCI_MCASP_XEVTCTL_REG, TXDATADMADIS);

	/* Only 44100 and 48000 are valid, both have the same setting */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXDIV(3));

	/* Enable the DIT */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_TXDITCTL_REG, DITEN);

	return 0;
}

static void davinci_mcasp_set_dma_params(struct snd_pcm_substream *substream,
					 struct davinci_audio_dev *dev,
					 int word_length, u8 fifo_level)
{
	if (dev->version == MCASP_VERSION_4) {
		struct omap_pcm_dma_data *dma_data =
					dev->dma_params[substream->stream];

		dma_data->packet_size = fifo_level;
	} else {
		struct davinci_pcm_dma_params *dma_params =
					dev->dma_params[substream->stream];

		dma_params->data_type = word_length >> 3;
		dma_params->fifo_level = fifo_level;

		if (dev->version == MCASP_VERSION_2 && !fifo_level)
			dma_params->acnt = 4;
		else
			dma_params->acnt = dma_params->data_type;
	}
}

static int davinci_mcasp_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *cpu_dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	int word_length;
	u8 fifo_level;
	u8 slots = dev->tdm_slots;
	u8 active_serializers;
	int channels = params_channels(params);
	int ret;

	active_serializers = (channels + slots - 1) / slots;

	if (davinci_hw_common_param(dev, substream->stream, channels) == -EINVAL)
		return -EINVAL;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		fifo_level = dev->txnumevt * active_serializers;
	else
		fifo_level = dev->rxnumevt * active_serializers;

	if (dev->op_mode == DAVINCI_MCASP_DIT_MODE)
		ret = davinci_hw_dit_param(dev);
	else
		ret = davinci_hw_param(dev, substream->stream, channels);

	if (ret)
		return ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S8:
		word_length = 8;
		break;

	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		word_length = 16;
		break;

	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		word_length = 24;
		break;

	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		word_length = 32;
		break;

	default:
		printk(KERN_WARNING "davinci-mcasp: unsupported PCM format");
		return -EINVAL;
	}

	dev->sample_bits = word_length;
	davinci_mcasp_set_dma_params(substream, dev, word_length, fifo_level);
	davinci_config_channel_size(dev, word_length);

	return 0;
}

static int davinci_mcasp_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *cpu_dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = davinci_mcasp_start(dev, substream->stream);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		davinci_mcasp_stop(dev, substream->stream);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int davinci_mcasp_hwrule_buffersize(struct snd_pcm_hw_params *params,
						struct snd_pcm_hw_rule *rule,
						int stream)
{
	struct snd_interval *buffer_size = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE);
	int channels = params_channels(params);
	int periods = params_periods(params);
	struct davinci_audio_dev *dev = rule->private;
	int i;
	u8 slots = dev->tdm_slots;
	u8 max_active_serializers = (channels + slots - 1) / slots;
	u8 num_ser = 0;
	u8 num_evt = 0;
	unsigned long step = 1;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < dev->num_serializer; i++) {
			if (dev->serial_dir[i] == TX_MODE &&
					num_ser < max_active_serializers)
				num_ser++;
		}
		num_evt = dev->txnumevt * num_ser;
	} else {
		for (i = 0; i < dev->num_serializer; i++) {
			if (dev->serial_dir[i] == RX_MODE &&
					num_ser < max_active_serializers)
				num_ser++;
		}
		num_evt = dev->rxnumevt * num_ser;
	}

	/*
	 * The buffersize (in samples), must be a multiple of num_evt.  The
	 * buffersize (in frames) is the product of the period_size and the
	 * number of periods. Therefore, the buffersize should be a multiple
	 * of the number of periods. The below finds the least common
	 * multiple of num_evt and channels (since the number of samples
	 * per frame is equal to the number of channels). It also makes sure
	 * that the resulting step value (LCM / channels) is a multiple of the
	 * number of periods.
	 */
	step = lcm((lcm(num_evt, channels) / channels), periods);

	return snd_interval_step(buffer_size, 0, step);
}

static int davinci_mcasp_hwrule_txbuffersize(struct snd_pcm_hw_params *params,
						  struct snd_pcm_hw_rule *rule)
{
	return davinci_mcasp_hwrule_buffersize(params, rule,
						SNDRV_PCM_STREAM_PLAYBACK);
}

static int davinci_mcasp_hwrule_rxbuffersize(struct snd_pcm_hw_params *params,
						  struct snd_pcm_hw_rule *rule)
{
	return davinci_mcasp_hwrule_buffersize(params, rule,
						SNDRV_PCM_STREAM_CAPTURE);
}

static int davinci_mcasp_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(dai);


	if (dev->version == MCASP_VERSION_4) {
		snd_soc_dai_set_dma_data(dai, substream,
					 dev->dma_params[substream->stream]);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (dev->txnumevt)
				snd_pcm_hw_rule_add(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
					davinci_mcasp_hwrule_txbuffersize,
					dev,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE, -1);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (dev->rxnumevt)
				snd_pcm_hw_rule_add(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
					davinci_mcasp_hwrule_rxbuffersize,
					dev,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE, -1);
		}
	}
	else
		snd_soc_dai_set_dma_data(dai, substream, dev->dma_params);

	/* Apply channels symmetry, needed when using a single serializer */
	if (dev->channels)
		snd_pcm_hw_constraint_minmax(substream->runtime,
					     SNDRV_PCM_HW_PARAM_CHANNELS,
					     dev->channels, dev->channels);

	/*
	 * Apply sample size symmetry since the slot size (defined by word
	 * length) has to be the same for playback and capture in McASP
	 * instances with unified clock/sync domain
	 */
	if (dev->sample_bits)
		snd_pcm_hw_constraint_minmax(substream->runtime,
					     SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
					     dev->sample_bits,
					     dev->sample_bits);

	dev->substreams[substream->stream] = substream;

	return 0;
}

static void davinci_mcasp_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (!dai->active) {
		dev->channels = 0;
		dev->sample_bits = 0;
	}

	dev->substreams[substream->stream] = NULL;
}

static const struct snd_soc_dai_ops davinci_mcasp_dai_ops = {
	.startup	= davinci_mcasp_startup,
	.shutdown	= davinci_mcasp_shutdown,
	.trigger	= davinci_mcasp_trigger,
	.hw_params	= davinci_mcasp_hw_params,
	.set_fmt	= davinci_mcasp_set_dai_fmt,
	.set_clkdiv	= davinci_mcasp_set_clkdiv,
	.set_sysclk	= davinci_mcasp_set_sysclk,
};

#define DAVINCI_MCASP_PCM_FMTS (SNDRV_PCM_FMTBIT_S8 | \
				SNDRV_PCM_FMTBIT_U8 | \
				SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_U16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_U24_LE | \
				SNDRV_PCM_FMTBIT_S24_3LE | \
				SNDRV_PCM_FMTBIT_U24_3LE | \
				SNDRV_PCM_FMTBIT_S32_LE | \
				SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_soc_dai_driver davinci_mcasp_dai[] = {
	{
		.name		= "davinci-mcasp.0",
		.playback	= {
			.channels_min	= 1,
			.channels_max	= 32 * 16,
			.rates 		= DAVINCI_MCASP_RATES,
			.formats	= DAVINCI_MCASP_PCM_FMTS,
		},
		.capture 	= {
			.channels_min	= 1,
			.channels_max	= 32 * 16,
			.rates 		= DAVINCI_MCASP_RATES,
			.formats	= DAVINCI_MCASP_PCM_FMTS,
		},
		.ops 		= &davinci_mcasp_dai_ops,

	},
	{
		"davinci-mcasp.1",
		.playback 	= {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= DAVINCI_MCASP_RATES,
			.formats	= DAVINCI_MCASP_PCM_FMTS,
		},
		.ops 		= &davinci_mcasp_dai_ops,
	},

};

static const struct of_device_id mcasp_dt_ids[] = {
	{
		.compatible = "ti,dm646x-mcasp-audio",
		.data = (void *)MCASP_VERSION_1,
	},
	{
		.compatible = "ti,da830-mcasp-audio",
		.data = (void *)MCASP_VERSION_2,
	},
	{
		.compatible = "ti,omap2-mcasp-audio",
		.data = (void *)MCASP_VERSION_3,
	},
	{
		.compatible = "ti,dra7-mcasp-audio",
		.data = (void *)MCASP_VERSION_4,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mcasp_dt_ids);

static struct snd_platform_data *davinci_mcasp_set_pdata_from_of(
						struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_platform_data *pdata = NULL;
	const struct of_device_id *match =
			of_match_device(of_match_ptr(mcasp_dt_ids), &pdev->dev);

	const u32 *of_serial_dir32;
	u8 *of_serial_dir;
	u32 val;
	int i, ret = 0;

	if (pdev->dev.platform_data) {
		pdata = pdev->dev.platform_data;
		return pdata;
	} else if (match) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto nodata;
		}
	} else {
		/* control shouldn't reach here. something is wrong */
		ret = -EINVAL;
		goto nodata;
	}

	if (match->data)
		pdata->version = (u8)((int)match->data);

	ret = of_property_read_u32(np, "op-mode", &val);
	if (ret >= 0)
		pdata->op_mode = val;

	ret = of_property_read_u32(np, "tdm-slots", &val);
	if (ret >= 0) {
		if (val < 2 || val > 32) {
			dev_err(&pdev->dev,
				"tdm-slots must be in rage [2-32]\n");
			ret = -EINVAL;
			goto nodata;
		}

		pdata->tdm_slots = val;
	}

	ret = of_property_read_u32(np, "num-serializer", &val);
	if (ret >= 0)
		pdata->num_serializer = val;

	of_serial_dir32 = of_get_property(np, "serial-dir", &val);
	val /= sizeof(u32);
	if (val != pdata->num_serializer) {
		dev_err(&pdev->dev,
				"num-serializer(%d) != serial-dir size(%d)\n",
				pdata->num_serializer, val);
		ret = -EINVAL;
		goto nodata;
	}

	if (of_serial_dir32) {
		of_serial_dir = devm_kzalloc(&pdev->dev,
						(sizeof(*of_serial_dir) * val),
						GFP_KERNEL);
		if (!of_serial_dir) {
			ret = -ENOMEM;
			goto nodata;
		}

		for (i = 0; i < pdata->num_serializer; i++)
			of_serial_dir[i] = be32_to_cpup(&of_serial_dir32[i]);

		pdata->serial_dir = of_serial_dir;
	}

	of_property_read_u32(np, "ti,tx-inactive-mode", &pdata->tx_dismod);

	of_property_read_u32(np, "ti,rx-inactive-mode", &pdata->rx_dismod);

	/* DISMOD = 1 is a reserved value */
	if ((pdata->tx_dismod == 1) || (pdata->rx_dismod == 1)) {
		dev_err(&pdev->dev, "tx/rx-inactive-mode cannot be 1\n");
		ret = -EINVAL;
		goto nodata;
	}

	if ((pdata->tx_dismod > 3) || (pdata->rx_dismod > 3)) {
		dev_err(&pdev->dev, "invalid tx/rx-inactive-mode\n");
		ret = -EINVAL;
		goto nodata;
	}

	ret = of_property_read_u32(np, "tx-num-evt", &val);
	if (ret >= 0)
		pdata->txnumevt = val;

	ret = of_property_read_u32(np, "rx-num-evt", &val);
	if (ret >= 0)
		pdata->rxnumevt = val;

	ret = of_property_read_u32(np, "sram-size-playback", &val);
	if (ret >= 0)
		pdata->sram_size_playback = val;

	ret = of_property_read_u32(np, "sram-size-capture", &val);
	if (ret >= 0)
		pdata->sram_size_capture = val;

	return  pdata;

nodata:
	if (ret < 0) {
		dev_err(&pdev->dev, "Error populating platform data, err %d\n",
			ret);
		pdata = NULL;
	}
	return  pdata;
}

static int davinci_mcasp_probe(struct platform_device *pdev)
{
	struct resource *mem, *ioarea, *mem_dat;
	struct resource *tx_res, *rx_res;
	struct snd_platform_data *pdata;
	struct davinci_audio_dev *dev;
	int irq;
	int ret;

	if (!pdev->dev.platform_data && !pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	dev = devm_kzalloc(&pdev->dev, sizeof(struct davinci_audio_dev),
			   GFP_KERNEL);
	if (!dev)
		return	-ENOMEM;

	pdata = davinci_mcasp_set_pdata_from_of(pdev);
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -ENODEV;
	}

	ioarea = devm_request_mem_region(&pdev->dev, mem->start,
			resource_size(mem), pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "Audio region already claimed\n");
		return -EBUSY;
	}

	irq = platform_get_irq_byname(pdev, "rx");
	if (irq >= 0) {
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				davinci_mcasp_rx_irq_handler,
				IRQF_ONESHOT, dev_name(&pdev->dev), dev);
		if (ret) {
			dev_err(&pdev->dev, "RX IRQ request failed\n");
			return ret;
		}
	}

	irq = platform_get_irq_byname(pdev, "tx");
	if (irq >= 0) {
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				davinci_mcasp_tx_irq_handler,
				IRQF_ONESHOT, dev_name(&pdev->dev), dev);
		if (ret) {
			dev_err(&pdev->dev, "TX IRQ request failed\n");
			return ret;
		}
	}

	mem_dat = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dat");
	if (!mem_dat) {
		dev_info(&pdev->dev, "data port resource not defined, cfg port will be used\n");
		dev->dat_port = false;
	} else {
		dev->dat_port = true;
	}

	pm_runtime_enable(&pdev->dev);

	dev->base = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!dev->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release_clk;
	}

	dev->op_mode = pdata->op_mode;
	dev->tdm_slots = pdata->tdm_slots;
	dev->num_serializer = pdata->num_serializer;
	dev->serial_dir = pdata->serial_dir;
	dev->version = pdata->version;
	dev->txnumevt = pdata->txnumevt;
	dev->rxnumevt = pdata->rxnumevt;
	dev->tx_dismod = pdata->tx_dismod;
	dev->rx_dismod = pdata->rx_dismod;
	dev->dev = &pdev->dev;

	/* first TX, then RX */
	tx_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!tx_res) {
		dev_err(&pdev->dev, "no DMA resource\n");
		return -ENODEV;
	}

	rx_res = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!rx_res) {
		dev_err(&pdev->dev, "no DMA resource\n");
		return -ENODEV;
	}

	if (dev->version == MCASP_VERSION_4) {
		struct omap_pcm_dma_data *dma_data;

		dma_data = devm_kzalloc(&pdev->dev, sizeof(*dma_data),
					GFP_KERNEL);
		if (!dma_data) {
			ret = -ENOMEM;
			goto err_release_clk;
		}

		dev->dma_params[SNDRV_PCM_STREAM_PLAYBACK] = dma_data;
		if (mem_dat)
			dma_data->port_addr = mem_dat->start;
		else
			dma_data->port_addr = mem->start + DAVINCI_MCASP_TXBUF_REG;
		dma_data->dma_req = tx_res->start;

		dma_data = devm_kzalloc(&pdev->dev, sizeof(*dma_data),
					GFP_KERNEL);
		if (!dma_data) {
			ret = -ENOMEM;
			goto err_release_clk;
		}

		dev->dma_params[SNDRV_PCM_STREAM_CAPTURE] = dma_data;
		if (mem_dat)
			dma_data->port_addr = mem_dat->start;
		else
			dma_data->port_addr = mem->start + DAVINCI_MCASP_RXBUF_REG;
		dma_data->dma_req = rx_res->start;
	} else {
		struct davinci_pcm_dma_params *dma_data;

		dma_data = devm_kzalloc(&pdev->dev, sizeof(*dma_data),
					GFP_KERNEL);
		if (!dma_data) {
			ret = -ENOMEM;
			goto err_release_clk;
		}

		dev->dma_params[SNDRV_PCM_STREAM_PLAYBACK] = dma_data;
		dma_data->asp_chan_q = pdata->asp_chan_q;
		dma_data->ram_chan_q = pdata->ram_chan_q;
		dma_data->sram_pool = pdata->sram_pool;
		dma_data->sram_size = pdata->sram_size_playback;
		dma_data->channel = tx_res->start;
		dma_data->dma_addr = (dma_addr_t)(pdata->tx_dma_offset +
						   mem->start);

		dma_data = devm_kzalloc(&pdev->dev, sizeof(*dma_data),
					GFP_KERNEL);
		if (!dma_data) {
			ret = -ENOMEM;
			goto err_release_clk;
		}

		dev->dma_params[SNDRV_PCM_STREAM_CAPTURE] = dma_data;
		dma_data->asp_chan_q = pdata->asp_chan_q;
		dma_data->ram_chan_q = pdata->ram_chan_q;
		dma_data->sram_pool = pdata->sram_pool;
		dma_data->sram_size = pdata->sram_size_capture;
		dma_data->channel = rx_res->start;
		dma_data->dma_addr = (dma_addr_t)(pdata->rx_dma_offset +
						  mem->start);
	}

	dev_set_drvdata(&pdev->dev, dev);
	ret = snd_soc_register_dai(&pdev->dev, &davinci_mcasp_dai[pdata->op_mode]);

	if (ret != 0)
		goto err_release_clk;

	if (dev->version != MCASP_VERSION_4) {
		ret = davinci_soc_platform_register(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "register PCM failed: %d\n", ret);
			goto err_unregister_dai;
		}
	}

	return 0;

err_unregister_dai:
	snd_soc_unregister_dai(&pdev->dev);
err_release_clk:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int davinci_mcasp_remove(struct platform_device *pdev)
{
	struct davinci_audio_dev *dev = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_dai(&pdev->dev);

	if (dev->version != MCASP_VERSION_4)
		davinci_soc_platform_unregister(&pdev->dev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver davinci_mcasp_driver = {
	.probe		= davinci_mcasp_probe,
	.remove		= davinci_mcasp_remove,
	.driver		= {
		.name	= "davinci-mcasp",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mcasp_dt_ids),
	},
};

module_platform_driver(davinci_mcasp_driver);

MODULE_AUTHOR("Steve Chen");
MODULE_DESCRIPTION("TI DAVINCI McASP SoC Interface");
MODULE_LICENSE("GPL");

