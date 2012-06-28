/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __TF_ZEBRA_H__
#define __TF_ZEBRA_H__

#include "tf_defs.h"

#ifdef CONFIG_TF_ION
extern struct ion_device *omap_ion_device;
#define zebra_ion_device omap_ion_device
#endif

int tf_ctrl_device_register(void);

int tf_start(struct tf_comm *comm,
	u32 workspace_addr, u32 workspace_size,
	u8 *pa_buffer, u32 pa_size,
	u32 conf_descriptor, u32 conf_offset, u32 conf_size);

/* Assembler entry points to/from secure */
u32 schedule_secure_world(u32 app_id, u32 proc_id, u32 flags, u32 args);
u32 rpc_handler(u32 p1, u32 p2, u32 p3, u32 p4);

void tf_clock_timer_init(void);
void tf_clock_timer_start(void);
void tf_clock_timer_stop(void);
u32 tf_try_disabling_secure_hwa_clocks(u32 mask);

#ifdef MODULE
extern int __initdata (*tf_comm_early_init)(void);
int __init tf_device_mshield_init(char *str);
void __exit tf_device_mshield_exit(void);
#endif

#endif /* __TF_ZEBRA_H__ */
