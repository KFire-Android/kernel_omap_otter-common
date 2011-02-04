/*
 * MMC definitions for OMAP2
 *
 * Copyright (C) 2006 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OMAP2_MMC_H
#define __OMAP2_MMC_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mmc/host.h>
#include <linux/platform_device.h>

#include <plat/board.h>
#include <plat/omap_hwmod.h>

#ifdef CONFIG_TIWLAN_SDIO
#include <linux/mmc/card.h>
#endif

#define OMAP15XX_NR_MMC		1
#define OMAP16XX_NR_MMC		2
#define OMAP1_MMC_SIZE		0x080
#define OMAP1_MMC1_BASE		0xfffb7800
#define OMAP1_MMC2_BASE		0xfffb7c00	/* omap16xx only */

#define OMAP24XX_NR_MMC		2
#define OMAP34XX_NR_MMC		3
#define OMAP44XX_NR_MMC		5
#define OMAP2420_MMC_SIZE	OMAP1_MMC_SIZE
#define OMAP3_HSMMC_SIZE	0x200
#define OMAP4_HSMMC_SIZE	0x1000
#define OMAP2_MMC1_BASE		0x4809c000
#define OMAP2_MMC2_BASE		0x480b4000
#define OMAP3_MMC3_BASE		0x480ad000
#define OMAP4_MMC4_BASE		0x480d1000
#define OMAP4_MMC5_BASE		0x480d5000
#define OMAP4_MMC_REG_OFFSET	0x100
#define HSMMC5			(1 << 4)
#define HSMMC4			(1 << 3)
#define HSMMC3			(1 << 2)
#define HSMMC2			(1 << 1)
#define HSMMC1			(1 << 0)

#define OMAP_MMC_MAX_SLOTS	2

#ifdef CONFIG_TIWLAN_SDIO
struct embedded_sdio_data {
	struct sdio_cis cis;
	struct sdio_cccr cccr;
	struct sdio_embedded_func *funcs;
	unsigned int quirks;
};
#endif

struct mmc_dev_attr {
	u8 flags;
};

enum {
	OMAP_HSMMC_SYSCONFIG = 0,
	OMAP_HSMMC_SYSSTATUS,
	OMAP_HSMMC_CON,
	OMAP_HSMMC_BLK,
	OMAP_HSMMC_ARG,
	OMAP_HSMMC_CMD,
	OMAP_HSMMC_RSP10,
	OMAP_HSMMC_RSP32,
	OMAP_HSMMC_RSP54,
	OMAP_HSMMC_RSP76,
	OMAP_HSMMC_DATA,
	OMAP_HSMMC_PSTATE,
	OMAP_HSMMC_HCTL,
	OMAP_HSMMC_SYSCTL,
	OMAP_HSMMC_STAT,
	OMAP_HSMMC_IE,
	OMAP_HSMMC_ISE,
	OMAP_HSMMC_CAPA,
	OMAP_HSMMC_CUR_CAPA,
	OMAP_HSMMC_FE,
	OMAP_HSMMC_ADMA_ES,
	OMAP_HSMMC_ADMA_SAL,
	OMAP_HSMMC_REV,
};

static const u16 omap3_mmc_reg_map[] = {
	[OMAP_HSMMC_SYSCONFIG] = 0x0010,
	[OMAP_HSMMC_SYSSTATUS] = 0x0014,
	[OMAP_HSMMC_CON] = 0x002C,
	[OMAP_HSMMC_BLK] = 0x0104,
	[OMAP_HSMMC_ARG] = 0x0108,
	[OMAP_HSMMC_CMD] = 0x010C,
	[OMAP_HSMMC_RSP10] = 0x0110,
	[OMAP_HSMMC_RSP32] = 0x0114,
	[OMAP_HSMMC_RSP54] = 0x0118,
	[OMAP_HSMMC_RSP76] = 0x011C,
	[OMAP_HSMMC_DATA] = 0x0120,
	[OMAP_HSMMC_PSTATE] = 0x0124,
	[OMAP_HSMMC_HCTL] = 0x0128,
	[OMAP_HSMMC_SYSCTL] = 0x012C,
	[OMAP_HSMMC_STAT] = 0x0130,
	[OMAP_HSMMC_IE] = 0x0134,
	[OMAP_HSMMC_ISE] = 0x0138,
	[OMAP_HSMMC_CAPA] = 0x0140,
};

static const u16 omap4_mmc_reg_map[] = {
	[OMAP_HSMMC_SYSCONFIG] = 0x0110,
	[OMAP_HSMMC_SYSSTATUS] = 0x0114,
	[OMAP_HSMMC_CON] = 0x012C,
	[OMAP_HSMMC_BLK] = 0x0204,
	[OMAP_HSMMC_ARG] = 0x0208,
	[OMAP_HSMMC_CMD] = 0x020C,
	[OMAP_HSMMC_RSP10] = 0x0210,
	[OMAP_HSMMC_RSP32] = 0x0214,
	[OMAP_HSMMC_RSP54] = 0x0218,
	[OMAP_HSMMC_RSP76] = 0x021C,
	[OMAP_HSMMC_DATA] = 0x0220,
	[OMAP_HSMMC_PSTATE] = 0x0224,
	[OMAP_HSMMC_HCTL] = 0x0228,
	[OMAP_HSMMC_SYSCTL] = 0x022C,
	[OMAP_HSMMC_STAT] = 0x0230,
	[OMAP_HSMMC_IE] = 0x0234,
	[OMAP_HSMMC_ISE] = 0x0238,
	[OMAP_HSMMC_CAPA] = 0x0240,
	[OMAP_HSMMC_CUR_CAPA] = 0x0248,
	[OMAP_HSMMC_FE] = 0x0250,
	[OMAP_HSMMC_ADMA_ES] = 0x0254,
	[OMAP_HSMMC_ADMA_SAL] = 0x0258,
	[OMAP_HSMMC_REV] = 0x02FC,
};

struct omap_mmc_platform_data {
	/* back-link to device */
	struct device *dev;

	/* number of slots per controller */
	unsigned nr_slots:2;

	/* Register Offset Map */
	u16 *regs_map;

	/* set if your board has components or wiring that limits the
	 * maximum frequency on the MMC bus */
	unsigned int max_freq;

	/* switch the bus to a new slot */
	int (*switch_slot)(struct device *dev, int slot);
	/* initialize board-specific MMC functionality, can be NULL if
	 * not supported */
	int (*init)(struct device *dev);
	void (*cleanup)(struct device *dev);
	void (*shutdown)(struct device *dev);

	/* To handle board related suspend/resume functionality for MMC */
	int (*suspend)(struct device *dev, int slot);
	int (*resume)(struct device *dev, int slot);

	/* add min bus tput constraint */
	int(*set_min_bus_tput)(struct device *dev, u8 agent_id, long r);

	/* Return context loss count due to PM states changing */
	int (*get_context_loss_count)(struct device *dev);

	u64 dma_mask;

	/* integration attributes from the omap_hwmod layer */
	struct mmc_dev_attr *dev_attr;

	struct omap_mmc_slot_data {

		/* 4/8 wires and any additional host capabilities
		 * need to OR'd all capabilities (ref. linux/mmc/host.h) */
		u8  wires;	/* Used for the MMC driver on omap1 and 2420 */
		u32 caps;	/* Used for the MMC driver on 2430 and later */

		/*
		 * nomux means "standard" muxing is wrong on this board, and
		 * that board-specific code handled it before common init logic.
		 */
		unsigned nomux:1;

		/* switch pin can be for card detect (default) or card cover */
		unsigned cover:1;

		/* use the internal clock */
		unsigned internal_clock:1;

		/* nonremovable e.g. eMMC */
		unsigned nonremovable:1;

		/* Try to sleep or power off when possible */
		unsigned power_saving:1;

		/* If using power_saving and the MMC power is not to go off */
		unsigned no_off:1;

		/* Regulator off remapped to sleep */
		unsigned vcc_aux_disable_is_sleep:1;

		/* we can put the features above into this variable */
#define HSMMC_HAS_PBIAS		(1 << 0)
#define HSMMC_HAS_UPDATED_RESET	(1 << 1)
#define HSMMC_DVFS_24MHZ_CONST	(1 << 2)
#define HSMMC_HAS_48MHZ_MASTER_CLK     (1 << 3)
		unsigned features;

		int switch_pin;			/* gpio (card detect) */
		int gpio_wp;			/* gpio (write protect) */

		int (*set_bus_mode)(struct device *dev, int slot, int bus_mode);
		int (*set_power)(struct device *dev, int slot,
				 int power_on, int vdd);
		int (*get_ro)(struct device *dev, int slot);
		int (*set_sleep)(struct device *dev, int slot, int sleep,
				 int vdd, int cardsleep);
		void (*remux)(struct device *dev, int slot, int power_on);
		/* Call back before enabling / disabling regulators */
		void (*before_set_reg)(struct device *dev, int slot,
				       int power_on, int vdd);
		/* Call back after enabling / disabling regulators */
		void (*after_set_reg)(struct device *dev, int slot,
				      int power_on, int vdd);

		/* return MMC cover switch state, can be NULL if not supported.
		 *
		 * possible return values:
		 *   0 - closed
		 *   1 - open
		 */
		int (*get_cover_state)(struct device *dev, int slot);

		const char *name;
		u32 ocr_mask;

		/* Card detection IRQs */
		int card_detect_irq;
		int (*card_detect)(struct device *dev, int slot);

		unsigned int ban_openended:1;

#ifdef CONFIG_TIWLAN_SDIO
		struct embedded_sdio_data *embedded_sdio;
		int (*register_status_notify)
			(void (*callback)(int card_present, void *dev_id),
			void *dev_id);
#endif

	} slots[OMAP_MMC_MAX_SLOTS];
};

/* called from board-specific card detection service routine */
extern void omap_mmc_notify_cover_event(struct device *dev, int slot,
					int is_closed);

#if	defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE) || \
	defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)
void omap1_init_mmc(struct omap_mmc_platform_data **mmc_data,
				int nr_controllers);
void omap2_init_mmc(struct omap_mmc_platform_data *mmc_data, int ctrl_nr);
int omap_mmc_add(const char *name, int id, unsigned long base,
				unsigned long size, unsigned int irq,
				struct omap_mmc_platform_data *data);
#else
static inline void omap1_init_mmc(struct omap_mmc_platform_data **mmc_data,
				int nr_controllers)
{
}
static inline void omap2_init_mmc(struct omap_mmc_platform_data **mmc_data,
				int nr_controllers)
{
}
static inline int omap_mmc_add(const char *name, int id, unsigned long base,
				unsigned long size, unsigned int irq,
				struct omap_mmc_platform_data *data)
{
	return 0;
}

#endif
#endif
