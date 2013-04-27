/*
 * OMAP Secure API infrastructure.
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/platform_device.h>

#include <asm/cacheflush.h>
#include <asm/memblock.h>

#include <mach/omap-secure.h>

#include "common.h"
#include "clockdomain.h"

static unsigned int ppa_service_0_index;
static phys_addr_t omap_secure_memblock_base;
/*
 * Secure dispatcher switching LOCK - unfortunately, this could
 * End up serializing dispatcher calls if invoked from two
 * CPUs simultaneously! but no simplier way to secure
 * API shifting.
 */
static DEFINE_SPINLOCK(_secure_dispatcher_lock);
/* My alternate dispatcher API */
static omap_secure_dispatcher_t _alternate_secure_dispatcher;
static int omap_secure_dispatcher_switch(omap_secure_dispatcher_t replacement);
static struct clockdomain *l4_secure_clkdm;

static struct omap_secure_platform_data omap_secure_data = {
	.disable_nonboot_cpus	= disable_nonboot_cpus,
	.enable_nonboot_cpus	= enable_nonboot_cpus,
	.sched_setaffinity	= sched_setaffinity,
	.sched_getaffinity	= sched_getaffinity,
	.clkdm_allow_idle	= clkdm_allow_idle,
	.clkdm_lookup		= clkdm_lookup,
	.clkdm_wakeup		= clkdm_wakeup,
	.mm			= &init_mm,
	.secure_dispatcher_replace = omap_secure_dispatcher_switch,
	.omap_secure_dispatcher = omap_secure_dispatcher,
};

/**
 * omap_sec_dispatcher: Routine to dispatch low power secure
 * service routines
 * @idx: The HAL API index
 * @flag: The flag indicating criticality of operation
 * @nargs: Number of valid arguments out of four.
 * @arg1, arg2, arg3 args4: Parameters passed to secure API
 *
 * Return the non-zero error value on failure.
 */
u32 omap_secure_dispatcher(u32 idx, u32 flag, u32 nargs, u32 arg1, u32 arg2,
							 u32 arg3, u32 arg4)
{
	u32 ret = 0;
	u32 param[5];
	unsigned long flags;

	/* If we have an alternate dispatcher api, use it, else use default */
	spin_lock_irqsave(&_secure_dispatcher_lock, flags);
	if (_alternate_secure_dispatcher) {
		ret = _alternate_secure_dispatcher(idx, flag, nargs, arg1,
						   arg2, arg3, arg4);
		spin_unlock_irqrestore(&_secure_dispatcher_lock, flags);
		return ret;
	}
	spin_unlock_irqrestore(&_secure_dispatcher_lock, flags);

	param[0] = nargs;
	param[1] = arg1;
	param[2] = arg2;
	param[3] = arg3;
	param[4] = arg4;

	if (!l4_secure_clkdm) {
		if (cpu_is_omap54xx())
			l4_secure_clkdm = clkdm_lookup("l4sec_clkdm");
		else
			l4_secure_clkdm = clkdm_lookup("l4_secure_clkdm");
	}

	if (!l4_secure_clkdm) {
		pr_err("%s: failed to get l4_secure_clkdm\n", __func__);
		return -EINVAL;
	}

	clkdm_wakeup(l4_secure_clkdm);

	/*
	 * Secure API needs physical address
	 * pointer for the parameters
	 */
	flush_cache_all();
	outer_clean_range(__pa(param), __pa(param + 5));
	ret = omap_smc2(idx, flag, __pa(param));

	clkdm_allow_idle(l4_secure_clkdm);

	return ret;
}

/**
 * omap_secure_dispatcher_switch() - replace default dispatcher function
 * @replacement: Replacement secure-dispatcher - provide NULL to restore
 *
 * Calling this function should be careful about the API call sequences
 * involved. IT IS IMPORTANT TO put in a secure dispatcher ONLY AFTER
 * required setups are done by caller. the pointer MUST BE VALID at all
 * times. call this API with NULL parameter to restore back to original
 * handling prior to making the call pointer invalid (e.g. driver probe
 * replacement and remove removal).
 */
static int omap_secure_dispatcher_switch(omap_secure_dispatcher_t replacement)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&_secure_dispatcher_lock, flags);
	if (_alternate_secure_dispatcher && replacement) {
		WARN(1, "Already have an alternate dispatcher %pF: %pF\n",
		     _alternate_secure_dispatcher, (void *)_RET_IP_);
		ret = -EINVAL;
	} else {
		pr_debug("%s: alternate secure dispatcher = %pF: %pF\n",
			 __func__, _alternate_secure_dispatcher,
			 (void *)_RET_IP_);
		_alternate_secure_dispatcher = replacement;
	}
	spin_unlock_irqrestore(&_secure_dispatcher_lock, flags);

	return ret;
}

/* Allocate the memory to save secure ram */
int __init omap_secure_ram_reserve_memblock(void)
{
	u32 size = OMAP_SECURE_RAM_STORAGE;

	size = ALIGN(size, SZ_1M);
	omap_secure_memblock_base = arm_memblock_steal(size, SZ_1M);

	return 0;
}

phys_addr_t omap_secure_ram_mempool_base(void)
{
	return omap_secure_memblock_base;
}

#ifdef CONFIG_CPU_PM
static int secure_notifier(struct notifier_block *self, unsigned long cmd,
			   void *v)
{
	switch (cmd) {
	case CPU_CLUSTER_PM_EXIT:
		/*
		 * Dummy dispatcher call after OSWR and OFF
		 * Restore the right return Kernel address (with MMU on) for
		 * subsequent calls to secure ROM. Otherwise the return address
		 * will be to a PA return address and the system will hang.
		 */
		omap_secure_dispatcher(ppa_service_0_index,
				       FLAG_START_CRITICAL,
				       0, 0, 0, 0, 0);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block secure_notifier_block = {
	.notifier_call = secure_notifier,
};

static int __init secure_pm_init(void)
{
	if (omap_type() != OMAP2_DEVICE_TYPE_GP)
		cpu_pm_register_notifier(&secure_notifier_block);

	if (cpu_is_omap44xx())
		ppa_service_0_index = OMAP4_PPA_SERVICE_0;
	else if (cpu_is_omap54xx())
		ppa_service_0_index = OMAP5_PPA_SERVICE_0;

	return 0;
}
early_initcall(secure_pm_init);
#endif

static struct platform_device omap_secure_device = {
	.name		= "tf_driver",
	.id		= -1,
	.dev = {
		.platform_data	= &omap_secure_data,
	},
	.num_resources = 0,
};

/**
 * omap_reserve_secure_workspace_addr() - reserves the secure workspace
 *
 * Sets the SMC secure_workspace address and size in the platform_data and
 * reserves the memory from being mapped to the Kernel memory space.
 */
void __init omap_reserve_secure_workspace_addr(void)
{
	phys_addr_t start, size;

	/* Dont need to do anything for GP devices */
#ifndef CONFIG_MACH_OMAP_4430_KC1
	if (OMAP2_DEVICE_TYPE_GP == omap_type())
#endif
		return;

	start = omap_secure_data.secure_workspace_addr;
	size = omap_secure_data.secure_workspace_size;

	/*
	 * SMC needs statically allocated memory not mapped by kernel.
	 * This is because the PPA will need to have a pre-knowledge of
	 * this address range for doing specific cleanups as needed. This
	 * may also imply(depending on SoC) additional firewalls setup
	 * from secure boot perspective.
	 */
	if (!start || !size)
		goto out;

	if (memblock_remove(start, size)) {
		WARN(1, "Unable to remove 0x%08x for size 0x%08x\n",
		     start, size);
		/* Reset the params */
		start = 0;
		size = 0;
	}
out:
	omap_secure_data.secure_workspace_addr = start;
	omap_secure_data.secure_workspace_size = size;
}

/**
 * omap_secure_set_secure_workspace_addr() - sets the secure workspace
 *						address for SMC PA
 * @secure_workspace_addr: physical workspace address
 *			within the 1st 1GB of address range
 * @secure_workspace_size: amount of memory reserved from being mapped to
 *			kernel memory space
 *
 * Meant to be used by board files to override the defaults for the platform
 * SMC secure workspace, this is to be used if the assumptions for the SoC
 * in consideration does not meet the requirements of the platform.
 * sets the SMC secure_workspace address and size in the platform_data and
 * setsup the parameters for reserving the memory from being mapped to the
 * Kernel memory space.
 *
 * This needs to be invoked before omap_reserve is invoked from the board
 * file.
 */
void __init omap_secure_set_secure_workspace_addr(
					   phys_addr_t secure_workspace_addr,
					   phys_addr_t secure_workspace_size)
{
	omap_secure_data.secure_workspace_addr = secure_workspace_addr;
	omap_secure_data.secure_workspace_size = secure_workspace_size;
}

static int __init omap_secure_init(void)
{
	return platform_device_register(&omap_secure_device);
}
arch_initcall(omap_secure_init);
