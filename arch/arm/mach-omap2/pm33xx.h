/*
 * AM33XX Power Management Routines
 *
 * Copyright (C) 2012 Texas Instruments Inc.
 * Vaibhav Bedia <vaibhav.bedia@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_PM33XX_H
#define __ARCH_ARM_MACH_OMAP2_PM33XX_H

#include "control.h"

#ifndef __ASSEMBLER__
struct wkup_m3_context {
	struct am33xx_ipc_data	ipc_data;
	struct device		*dev;
	struct firmware		*firmware;
	struct mailbox		*mbox;
	void __iomem		*code;
	u8			state;
};

#ifdef CONFIG_SUSPEND
static void am33xx_m3_state_machine_reset(void);
#else
static inline void am33xx_m3_state_machine_reset(void) {}
#endif /* CONFIG_SUSPEND */

extern void __iomem *am33xx_get_emif_base(void);
#endif

#define	IPC_CMD_DS0			0x3
#define IPC_CMD_RESET                   0xe
#define DS_IPC_DEFAULT			0xffffffff

#define IPC_RESP_SHIFT			16
#define IPC_RESP_MASK			(0xffff << 16)

#define M3_STATE_UNKNOWN		0
#define M3_STATE_RESET			1
#define M3_STATE_INITED			2
#define M3_STATE_MSG_FOR_LP		3
#define M3_STATE_MSG_FOR_RESET		4

#define AM33XX_OCMC_END			0x40310000
#define AM33XX_EMIF_BASE		0x4C000000

#endif
