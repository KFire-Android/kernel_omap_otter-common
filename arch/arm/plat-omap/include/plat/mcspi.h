#ifndef _OMAP2_MCSPI_H
#define _OMAP2_MCSPI_H

#if defined CONFIG_ARCH_OMAP2PLUS
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>

#define OMAP2_MCSPI_MASTER		0
#define OMAP2_MCSPI_SLAVE		1

/**
 * struct omap2_mcspi_platform_config - McSPI controller configuration
 * @num_cs: Number of chip selects or channels supported
 * @mode: SPI is master or slave
 * @dma_mode: Use only DMA for data transfers
 * @force_cs_mode: Use force chip select mode or auto chip select mode
 * @fifo_depth: FIFO depth in bytes, max value 64
 *
 * @dma_mode when set to 1 uses only dma for data transfers
 * else the default behaviour is to use PIO mode for transfer
 * size of 8 bytes or less. This mode is useful when mcspi
 * is configured as slave
 *
 * @force_cs_mode when set to 1 allows continuous transfer of multiple
 * spi words without toggling the chip select line.
 *
 * @fifo_depth when set to non zero values enables FIFO. fifo_depth
 * should be set as a multiple of buffer size used for read/write.
 */

struct omap2_mcspi_platform_config {
	u8	num_cs;
	u8	mode;
	u8	dma_mode;
	u8	force_cs_mode;
	unsigned short fifo_depth;
	u16	*regs_data;
};

struct omap2_mcspi_device_config {
	unsigned turbo_mode:1;

	/* Do we want one channel enabled at the same time? */
	unsigned single_channel:1;
};

enum {
	OMAP2_MCSPI_REVISION = 0,
	OMAP2_MCSPI_SYSCONFIG,
	OMAP2_MCSPI_SYSSTATUS,
	OMAP2_MCSPI_IRQSTATUS,
	OMAP2_MCSPI_IRQENABLE,
	OMAP2_MCSPI_WAKEUPENABLE,
	OMAP2_MCSPI_SYST,
	OMAP2_MCSPI_MODULCTRL,
	OMAP2_MCSPI_XFERLEVEL,
	OMAP2_MCSPI_CHCONF0,
	OMAP2_MCSPI_CHSTAT0,
	OMAP2_MCSPI_CHCTRL0,
	OMAP2_MCSPI_TX0,
	OMAP2_MCSPI_RX0,
	OMAP2_MCSPI_HL_REV,
	OMAP2_MCSPI_HL_HWINFO,
	OMAP2_MCSPI_HL_SYSCONFIG,
};

static const u16 omap2_reg_map[] = {
	[OMAP2_MCSPI_REVISION]		= 0x00,
	[OMAP2_MCSPI_SYSCONFIG]		= 0x10,
	[OMAP2_MCSPI_SYSSTATUS]		= 0x14,
	[OMAP2_MCSPI_IRQSTATUS]		= 0x18,
	[OMAP2_MCSPI_IRQENABLE]		= 0x1c,
	[OMAP2_MCSPI_WAKEUPENABLE]	= 0x20,
	[OMAP2_MCSPI_SYST]		= 0x24,
	[OMAP2_MCSPI_MODULCTRL]		= 0x28,
	[OMAP2_MCSPI_XFERLEVEL]		= 0x7c,
	[OMAP2_MCSPI_CHCONF0]		= 0x2c,
	[OMAP2_MCSPI_CHSTAT0]		= 0x30,
	[OMAP2_MCSPI_CHCTRL0]		= 0x34,
	[OMAP2_MCSPI_TX0]		= 0x38,
	[OMAP2_MCSPI_RX0]		= 0x3c,
};

static const u16 omap4_reg_map[] = {
	[OMAP2_MCSPI_REVISION]		= 0x100,
	[OMAP2_MCSPI_SYSCONFIG]		= 0x110,
	[OMAP2_MCSPI_SYSSTATUS]		= 0x114,
	[OMAP2_MCSPI_IRQSTATUS]		= 0x118,
	[OMAP2_MCSPI_IRQENABLE]		= 0x01c,
	[OMAP2_MCSPI_WAKEUPENABLE]	= 0x120,
	[OMAP2_MCSPI_SYST]		= 0x124,
	[OMAP2_MCSPI_MODULCTRL]		= 0x128,
	[OMAP2_MCSPI_XFERLEVEL]		= 0x17c,
	[OMAP2_MCSPI_CHCONF0]		= 0x12c,
	[OMAP2_MCSPI_CHSTAT0]		= 0x130,
	[OMAP2_MCSPI_CHCTRL0]		= 0x134,
	[OMAP2_MCSPI_TX0]		= 0x138,
	[OMAP2_MCSPI_RX0]		= 0x13c,
	[OMAP2_MCSPI_HL_REV]		= 0x000,
	[OMAP2_MCSPI_HL_HWINFO]		= 0x004,
	[OMAP2_MCSPI_HL_SYSCONFIG]	= 0x010,
};

#endif

#endif
