/*
 * hdcp.h
 *
 * HDCP interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Sujeet Baranwal
 *	Sujeet Baranwal <s-obaranwal@ti.com>
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

#ifndef _HDCP_H_
#define _HDCP_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/********************************/
/* Structures related to ioctl  */
/********************************/

/* HDCP key size in 32-bit words */
#define DESHDCP_KEY_SIZE 160

#define MAX_SHA_DATA_SIZE	645
#define MAX_SHA_VPRIME_SIZE	20

struct hdcp_encrypt_control {
	uint32_t in_key[DESHDCP_KEY_SIZE];
	uint32_t *out_key;
};

struct hdcp_enable_control {
	uint32_t key[DESHDCP_KEY_SIZE];
	int nb_retry;
};

struct hdcp_sha_in {
	uint8_t data[MAX_SHA_DATA_SIZE];
	uint32_t byte_counter;
	uint8_t vprime[MAX_SHA_VPRIME_SIZE];
};

struct hdcp_wait_control {
	uint32_t event;
	struct hdcp_sha_in *data;
};

/* HDCP ioctl */

#define HDCP_IOCTL_MAGIC 'h'

#define HDCP_WAIT_EVENT _IOWR(HDCP_IOCTL_MAGIC, 0, \
				struct hdcp_wait_control)
#define HDCP_DONE	_IOW(HDCP_IOCTL_MAGIC, 1, uint32_t)
#define HDCP_QUERY_STATUS _IOWR(HDCP_IOCTL_MAGIC, 2, uint32_t)

/* Status / error codes */
#define HDCP_OK			0
#define HDCP_3DES_ERROR		1

#define HDMI_HDCP_ENABLED	0x1
#define HDMI_HDCP_FAILED	0x0

/* HDCP events */
#define HDCP_EVENT_STEP2	(1 << 0x1)
#define HDCP_EVENT_EXIT		(1 << 0x2)

/* HDCP user space status */
#define HDCP_US_NO_ERR		(0 << 8)
#define HDCP_US_FAILURE		(1 << 8)

/* HDCP interrupts bits */
#define KSVACCESSINT		(1 << 0x0)
#define KSVSHA1CALCINT		(1 << 0x1)
#define KEEPOUTERRORINT		(1 << 0x2)
#define LOSTARBITRATION		(1 << 0x3)
#define I2CNACK			(1 << 0x4)
#define HDCP_FAILED		(1 << 0x6)
#define HDCP_ENGAGED		(1 << 0x7)

#ifdef __KERNEL__

#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/fs.h>

/***************************/
/* HW specific definitions */
/***************************/

/* DESHDCP base address */
/*----------------------*/
#define DSS_SS_FROM_L3__DESHDCP 0x58007000

#define HDMI_CORE 0x58060000

/* DESHDCP registers */
#define DESHDCP__DHDCP_CTRL   0x020
#define DESHDCP__DHDCP_DATA_L 0x024
#define DESHDCP__DHDCP_DATA_H 0x028

/* DESHDCP CTRL bits */
#define DESHDCP__DHDCP_CTRL__DIRECTION_POS_F 2
#define DESHDCP__DHDCP_CTRL__DIRECTION_POS_L 2

#define DESHDCP__DHDCP_CTRL__OUTPUT_READY_POS_F 0
#define DESHDCP__DHDCP_CTRL__OUTPUT_READY_POS_L 0

/***************************/
/* Definitions             */
/***************************/

/***************************/
/* Macros for accessing HW */
/***************************/
#define WR_REG_32(base, offset, val)	__raw_writel(val, base + offset)
#define RD_REG_32(base, offset)		__raw_readl(base + offset)

#define WR_FIELD_32(base, offset, start, end, val) \
	WR_REG_32(base, offset, FLD_MOD(RD_REG_32(base, offset), val, \
		  start, end))

#define RD_FIELD_32(base, offset, start, end) \
	((RD_REG_32(base, offset) & FLD_MASK(start, end)) >> (end))

#undef DBG

#define HDCP_ERR(format, ...) \
		printk(KERN_ERR "HDCP: " format "\n", ## __VA_ARGS__)

#define HDCP_INFO(format, ...) \
		printk(KERN_INFO "HDCP: " format "\n", ## __VA_ARGS__)

#define HDCP_WARN(format, ...) \
		printk(KERN_WARNING "HDCP: " format "\n", ## __VA_ARGS__)
#ifdef HDCP_DEBUG
#define HDCP_DBG(format, ...) \
		printk(KERN_DEBUG "HDCP: " format "\n", ## __VA_ARGS__)
#else
#define HDCP_DBG(format, ...)
#endif

#endif /* __KERNEL__ */

#endif /* _HDCP_H_ */
