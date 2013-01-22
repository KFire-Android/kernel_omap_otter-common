/*
 * omap-secure.h: OMAP Secure infrastructure header.
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef OMAP_ARCH_OMAP_SECURE_H
#define OMAP_ARCH_OMAP_SECURE_H

/* Monitor error code */
#define  API_HAL_RET_VALUE_NS2S_CONVERSION_ERROR	0xFFFFFFFE
#define  API_HAL_RET_VALUE_SERVICE_UNKNWON		0xFFFFFFFF

/* HAL API error codes */
#define  API_HAL_RET_VALUE_OK		0x00
#define  API_HAL_RET_VALUE_FAIL		0x01

/* Secure HAL API flags */
#define FLAG_START_CRITICAL		0x4
#define FLAG_IRQFIQ_MASK		0x3
#define FLAG_IRQ_ENABLE			0x2
#define FLAG_FIQ_ENABLE			0x1
#define NO_FLAG				0x0

/* Maximum Secure memory storage size */
#define OMAP_SECURE_RAM_STORAGE	(88 * SZ_1K)

/* Secure low power HAL API index */
#define OMAP4_HAL_SAVESECURERAM_INDEX	0x1a
#define OMAP4_HAL_SAVEHW_INDEX		0x1b
#define OMAP4_HAL_SAVEALL_INDEX		0x1c
#define OMAP4_HAL_SAVEGIC_INDEX		0x1d
#define OMAP5_HAL_SAVESECURERAM_INDEX	0x1c
#define OMAP5_HAL_SAVEHW_INDEX		0x1d
#define OMAP5_HAL_SAVEALL_INDEX		0x1e
#define OMAP5_HAL_SAVEGIC_INDEX		0x1f

/* Secure Monitor mode APIs */
#define OMAP4_MON_SCU_PWR_INDEX		0x108
#define OMAP4_MON_L2X0_DBG_CTRL_INDEX	0x100
#define OMAP4_MON_L2X0_CTRL_INDEX	0x102
#define OMAP4_MON_L2X0_AUXCTRL_INDEX	0x109
#define OMAP4_MON_L2X0_PREFETCH_INDEX	0x113
#define OMAP5_MON_CACHES_CLEAN_INDEX	0x103
#define OMAP5_MON_AUX_CTRL_INDEX	0x107
#define OMAP5_MON_L2AUX_CTRL_INDEX	0x104

/* Secure PPA(Primary Protected Application) APIs */
#define OMAP4_PPA_SERVICE_0		0x21
#define OMAP4_PPA_L2_POR_INDEX		0x23
#define OMAP4_PPA_CPU_ACTRL_SMP_INDEX	0x25
#define OMAP5_PPA_SERVICE_0		0x201

#ifndef __ASSEMBLER__

extern u32 omap_secure_dispatcher(u32 idx, u32 flag, u32 nargs,
				u32 arg1, u32 arg2, u32 arg3, u32 arg4);
extern u32 omap_smc2(u32 id, u32 falg, u32 pargs);
extern phys_addr_t omap_secure_ram_mempool_base(void);

typedef u32 (*omap_secure_dispatcher_t)(u32 idx, u32 flag,
			u32 nargs, u32 arg1, u32 arg2, u32 arg3, u32 arg4);
struct clockdomain;
/**
 * struct omap_secure_platform_data -  For Secure driver to use
 * @disable_nonboot_cpus:	standard linux API
 * @enable_nonboot_cpus:	standard linux API
 * @sched_setaffinity:		standard linux API
 * @sched_getaffinity:		standard linux API
 * @clkdm_allow_idle:		standard omap SoC API
 * @clkdm_lookup:		standard omap SoC API
 * @clkdm_wakeup:		standard omap SoC API
 * @secure_dispatcher_replace:	API to replace standard dispatcher by alternate
 * @omap_dispatcher_replace:	standard omap secure dispatcher
 * @mm:				standard linux init_mm struct
 * @secure_workspace_addr:	smc physical workspace address within the 1st GB
 * @secure_workspace_size:	size of workspace reserved frm being
 *				mapped to kernel memory space
 *
 * An standin structure to expose Linux kernel internal private
 * structure to Security Driver.
 * XXX: MOST OF THESE APIS should eventually disappear, but a first
 * step towards that.
 */
struct omap_secure_platform_data {
	int (*disable_nonboot_cpus)(void);
	void (*enable_nonboot_cpus)(void);
	long (*sched_setaffinity)(pid_t pid, const struct cpumask *new_mask);
	long (*sched_getaffinity)(pid_t pid, struct cpumask *mask);
	void (*clkdm_allow_idle)(struct clockdomain *clkdm);
	struct clockdomain* (*clkdm_lookup)(const char *name);
	int (*clkdm_wakeup)(struct clockdomain *clkdm);
	int (*secure_dispatcher_replace) (omap_secure_dispatcher_t replacement);
	u32 (*omap_secure_dispatcher)(u32 idx, u32 flag, u32 nargs,
				      u32 arg1, u32 arg2, u32 arg3, u32 arg4);
	struct mm_struct *mm;
	phys_addr_t secure_workspace_addr;
	u32 secure_workspace_size;
};

void omap_secure_set_secure_workspace_addr(phys_addr_t secure_workspace_addr,
					   phys_addr_t secure_workspace_size);
void omap_reserve_secure_workspace_addr(void);

/*
 * OMAP5 defaults assume atleast 1GB availability
 * For platforms that do not meet this need or use modified PPA
 * different from TI defaults, override from board file
 */
#define OMAP5_SECURE_WORKSPACE_BASE	0xBFA00000
#define OMAP5_SECURE_WORKSPACE_SIZE	(6 * SZ_1M)

/**
 * omap5_secure_workspace_addr_default() - set default OMAP5 SMC address and size
 *
 * cpu_is_omapXYZ() macros dont work in reserve due to bad init sequence in OMAP
 * framework (works only after io and early inits are called). So SoC specific
 * APIs to help board files... :(
 */
static inline void omap5_secure_workspace_addr_default(void)
{
	omap_secure_set_secure_workspace_addr(OMAP5_SECURE_WORKSPACE_BASE,
					      OMAP5_SECURE_WORKSPACE_SIZE);
}

/*
 * OMAP4 defaults assume atleast 1GB availability
 * For platforms that do not meet this need or use modified PPA
 * different from TI defaults, override from board file
 */
#define OMAP4_SECURE_WORKSPACE_BASE	0xBFD00000
#define OMAP4_SECURE_WORKSPACE_SIZE	(3 * SZ_1M)

/**
 * omap4_secure_workspace_addr_default() - set default OMAP4 SMC address and size
 *
 * cpu_is_omapXYZ() macros dont work in reserve due to bad init sequence in OMAP
 * framework (works only after io and early inits are called). So SoC specific
 * APIs to help board files... :(
 */
static inline void omap4_secure_workspace_addr_default(void)
{
	omap_secure_set_secure_workspace_addr(OMAP4_SECURE_WORKSPACE_BASE,
					      OMAP4_SECURE_WORKSPACE_SIZE);
}

#endif /* __ASSEMBLER__ */
#endif /* OMAP_ARCH_OMAP_SECURE_H */
