/*
 * OMAP2/3/4 powerdomain control
 *
 * Copyright (C) 2007-2008, 2010 Texas Instruments, Inc.
 * Copyright (C) 2007-2011 Nokia Corporation
 *
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * XXX This should be moved to the mach-omap2/ directory at the earliest
 * opportunity.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_POWERDOMAIN_H
#define __ARCH_ARM_MACH_OMAP2_POWERDOMAIN_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/plist.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#include <plat/cpu.h>

#include "voltage.h"

/* Powerdomain basic power states */
#define PWRDM_POWER_OFF		0x0
#define PWRDM_POWER_INACTIVE	0x2
#define PWRDM_POWER_ON		0x3
#define PWRDM_POWER_CSWR	0x4
#define PWRDM_POWER_OSWR	0x5

#define PWRDM_MAX_PWRSTS	4

/* Maximum number of power domain states - including OSWR */
#define PWRDM_MAX_POWER_PWRSTS	6

/* Unsupported power latency state for a power domain */
#define UNSUP_STATE		-1

/* Powerdomain allowable state bitfields */
#define PWRSTS_ON		(1 << PWRDM_POWER_ON)
#define PWRSTS_INACTIVE		(1 << PWRDM_POWER_INACTIVE)
#define PWRSTS_RET		(1 << PWRDM_POWER_RET)
#define PWRSTS_OFF		(1 << PWRDM_POWER_OFF)

#define PWRSTS_OFF_ON		(PWRSTS_OFF | PWRSTS_ON)
#define PWRSTS_OFF_RET		(PWRSTS_OFF | PWRSTS_RET)
#define PWRSTS_RET_ON		(PWRSTS_RET | PWRSTS_ON)
#define PWRSTS_RET_INA_ON	(PWRSTS_RET_ON | PWRSTS_INACTIVE)
#define PWRSTS_OFF_RET_ON	(PWRSTS_OFF_RET | PWRSTS_ON)
#define PWRSTS_OFF_INA_ON	(PWRSTS_OFF_ON | PWRSTS_INACTIVE)
#define PWRSTS_OFF_RET_INA_ON	(PWRSTS_OFF_RET_ON | PWRSTS_INACTIVE)

/* Powerdomain flags: hardware save-and-restore support */
#define PWRDM_HAS_HDWR_SAR		(1 << 0)
/*
 * MPU pwr domain has MEM bank 0 bits in MEM bank 1 position.
 * This is true for OMAP3430
 */
#define PWRDM_HAS_MPU_QUIRK		(1 << 1)
/*
 * Support to transition from a sleep state to a lower sleep state
 * without waking up the powerdomain
 */
#define PWRDM_HAS_LOWPOWERSTATECHANGE	(1 << 2)
/*
 * Supported only on CPU power domains to take care of Cortex-A15 based
 * design limitation. All the CPUs in a cluster transitions to low power
 * state together and not individually with wfi. Force OFF mode fix that
 * limitation and let CPU individually hit OFF mode.
 */
#define PWRDM_HAS_FORCE_OFF		(1 << 3)

/*
 * Number of memory banks that are power-controllable.	On OMAP4430, the
 * maximum is 5.
 */
#define PWRDM_MAX_MEM_BANKS	5

/*
 * Maximum number of clockdomains that can be associated with a powerdomain.
 * CORE powerdomain on OMAP4 is the worst case
 */
#define PWRDM_MAX_CLKDMS	9

#ifdef CONFIG_MACH_OMAP_5430ZEBU
#define PWRDM_TRANSITION_BAILOUT 1000
#else
/* XXX A completely arbitrary number. What is reasonable here? */
#define PWRDM_TRANSITION_BAILOUT 100000
#endif

struct clockdomain;
struct powerdomain;

/**
 * struct powerdomain - OMAP powerdomain
 * @name: Powerdomain name
 * @voltdm: voltagedomain containing this powerdomain
 * @prcm_offs: the address offset from CM_BASE/PRM_BASE
 * @context_offs: the address offset for the CONTEXT register
 * @prcm_partition: (OMAP4 only) the PRCM partition ID containing @prcm_offs
 * @pwrsts: Possible powerdomain power states
 * @pwrsts_logic_ret: Possible logic power states when pwrdm in RETENTION
 * @flags: Powerdomain flags
 * @banks: Number of software-controllable memory banks in this powerdomain
 * @pwrsts_mem_ret: Possible memory bank pwrstates when pwrdm in RETENTION
 * @pwrsts_mem_on: Possible memory bank pwrstates when pwrdm in ON
 * @pwrdm_clkdms: Clockdomains in this powerdomain
 * @node: list_head linking all powerdomains
 * @voltdm_node: list_head linking all powerdomains in a voltagedomain
 * @state:
 * @state_counter:
 * @timer:
 * @state_timer:
 * @usecount: powerdomain usecount
 * @lock: lock to keep the pwrdm structure access sanity.
 * @wkup_lat_plist_lock: Lock to control access to Latency and updates
 *
 * @prcm_partition possible values are defined in mach-omap2/prcm44xx.h.
 */
struct powerdomain {
	const char *name;
	union {
		const char *name;
		struct voltagedomain *ptr;
	} voltdm;
	const s16 prcm_offs;
	const s16 context_offs;
	const u8 pwrsts;
	const u8 pwrsts_logic_ret;
	const u8 flags;
	const u8 banks;
	const u8 pwrsts_mem_ret[PWRDM_MAX_MEM_BANKS];
	const u8 pwrsts_mem_on[PWRDM_MAX_MEM_BANKS];
	const u8 prcm_partition;
	struct clockdomain *pwrdm_clkdms[PWRDM_MAX_CLKDMS];
	struct list_head node;
	struct list_head voltdm_node;
	/* Lock to secure accesses around pwrdm data structure updates */
	spinlock_t lock;
	int state;
	unsigned state_counter[PWRDM_MAX_PWRSTS];
	unsigned ret_logic_off_counter;
	unsigned ret_mem_off_counter[PWRDM_MAX_MEM_BANKS];
	atomic_t usecount;

#ifdef CONFIG_PM_DEBUG
	s64 timer;
	s64 state_timer[PWRDM_MAX_PWRSTS];
#endif
	const s32 wakeup_lat[PWRDM_MAX_POWER_PWRSTS];
	struct plist_head wkup_lat_plist_head;
	/* Lock to control access to latency list */
	struct mutex wkup_lat_plist_lock;
	int wkup_lat_next_state;
};

/* Linked list for the wake-up latency constraints */
struct pwrdm_wkup_constraints_entry {
	void			*cookie;
	struct plist_node	node;
};

/**
 * struct pwrdm_ops - Arch specific function implementations
 * @pwrdm_set_next_pwrst: Set the target power state for a pd
 * @pwrdm_read_next_pwrst: Read the target power state set for a pd
 * @pwrdm_read_pwrst: Read the current power state of a pd
 * @pwrdm_read_prev_pwrst: Read the prev power state entered by the pd
 * @pwrdm_set_logic_retst: Set the logic state in RET for a pd
 * @pwrdm_set_mem_onst: Set the Memory state in ON for a pd
 * @pwrdm_set_mem_retst: Set the Memory state in RET for a pd
 * @pwrdm_read_logic_pwrst: Read the current logic state of a pd
 * @pwrdm_read_prev_logic_pwrst: Read the previous logic state entered by a pd
 * @pwrdm_read_logic_retst: Read the logic state in RET for a pd
 * @pwrdm_read_mem_pwrst: Read the current memory state of a pd
 * @pwrdm_read_prev_mem_pwrst: Read the previous memory state entered by a pd
 * @pwrdm_read_mem_retst: Read the memory state in RET for a pd
 * @pwrdm_clear_all_prev_pwrst: Clear all previous power states logged for a pd
 * @pwrdm_enable_hdwr_sar: Enable Hardware Save-Restore feature for the pd
 * @pwrdm_disable_hdwr_sar: Disable Hardware Save-Restore feature for a pd
 * @pwrdm_set_lowpwrstchange: Enable pd transitions from a shallow to deep sleep
 * @pwrdm_wait_transition: Wait for a pd state transition to complete
 * @pwrdm_enable_force_off: Enable force off transition feature for the pd
 * @pwrdm_disable_force_off: Disable force off transition feature for the pd
 * @pwrdm_lost_context_rff: Check if pd has lost RFF context (entered off)
 * @pwrdm_lost_context_rff: Check if pd has lost RFF context (omap4+ device off)
 * @pwrdm_enable_off: Extra off mode enable for pd (omap4+ device off)
 * @pwrdm_read_next_off: Check if pd next state is off (omap4+ device off)
 */
struct pwrdm_ops {
	int	(*pwrdm_set_next_pwrst)(struct powerdomain *pwrdm, u8 pwrst);
	int	(*pwrdm_read_next_pwrst)(struct powerdomain *pwrdm);
	int	(*pwrdm_read_pwrst)(struct powerdomain *pwrdm);
	int	(*pwrdm_read_prev_pwrst)(struct powerdomain *pwrdm);
	int	(*pwrdm_set_logic_retst)(struct powerdomain *pwrdm, u8 pwrst);
	int	(*pwrdm_set_mem_onst)(struct powerdomain *pwrdm, u8 bank, u8 pwrst);
	int	(*pwrdm_set_mem_retst)(struct powerdomain *pwrdm, u8 bank, u8 pwrst);
	int	(*pwrdm_read_logic_pwrst)(struct powerdomain *pwrdm);
	int	(*pwrdm_read_prev_logic_pwrst)(struct powerdomain *pwrdm);
	int	(*pwrdm_read_logic_retst)(struct powerdomain *pwrdm);
	int	(*pwrdm_read_mem_pwrst)(struct powerdomain *pwrdm, u8 bank);
	int	(*pwrdm_read_prev_mem_pwrst)(struct powerdomain *pwrdm, u8 bank);
	int	(*pwrdm_read_mem_retst)(struct powerdomain *pwrdm, u8 bank);
	int	(*pwrdm_clear_all_prev_pwrst)(struct powerdomain *pwrdm);
	int	(*pwrdm_enable_hdwr_sar)(struct powerdomain *pwrdm);
	int	(*pwrdm_disable_hdwr_sar)(struct powerdomain *pwrdm);
	int	(*pwrdm_set_lowpwrstchange)(struct powerdomain *pwrdm);
	int	(*pwrdm_wait_transition)(struct powerdomain *pwrdm);
	int	(*pwrdm_enable_force_off)(struct powerdomain *pwrdm);
	int	(*pwrdm_disable_force_off)(struct powerdomain *pwrdm);
	bool	(*pwrdm_lost_context_rff)(struct powerdomain *pwrdm);
	void	(*pwrdm_enable_off)(bool enable);
	bool	(*pwrdm_read_next_off)(void);
};

int pwrdm_register_platform_funcs(struct pwrdm_ops *custom_funcs);
int pwrdm_register_pwrdms(struct powerdomain **pwrdm_list);
int pwrdm_complete_init(void);

struct powerdomain *pwrdm_lookup(const char *name);

int pwrdm_for_each(int (*fn)(struct powerdomain *pwrdm, void *user),
			void *user);
int pwrdm_for_each_nolock(int (*fn)(struct powerdomain *pwrdm, void *user),
			void *user);

int pwrdm_add_clkdm(struct powerdomain *pwrdm, struct clockdomain *clkdm);
int pwrdm_del_clkdm(struct powerdomain *pwrdm, struct clockdomain *clkdm);
int pwrdm_for_each_clkdm(struct powerdomain *pwrdm,
			 int (*fn)(struct powerdomain *pwrdm,
				   struct clockdomain *clkdm));
struct voltagedomain *pwrdm_get_voltdm(struct powerdomain *pwrdm);

int pwrdm_get_mem_bank_count(struct powerdomain *pwrdm);

int pwrdm_get_achievable_pwrst(struct powerdomain *pwrdm, u8 req_pwrst);

int omap_set_pwrdm_state(struct powerdomain *pwrdm, u32 state);

int pwrdm_set_next_pwrst(struct powerdomain *pwrdm, u8 pwrst);
int pwrdm_read_next_pwrst(struct powerdomain *pwrdm);
int pwrdm_read_pwrst(struct powerdomain *pwrdm);
int pwrdm_read_prev_pwrst(struct powerdomain *pwrdm);
int pwrdm_clear_all_prev_pwrst(struct powerdomain *pwrdm);
int pwrdm_read_device_off_state(void);
void pwrdm_enable_off_mode(bool enable);

int pwrdm_enable_hdwr_sar(struct powerdomain *pwrdm);
int pwrdm_disable_hdwr_sar(struct powerdomain *pwrdm);
bool pwrdm_has_hdwr_sar(struct powerdomain *pwrdm);

int pwrdm_wait_transition(struct powerdomain *pwrdm);

int pwrdm_state_switch(struct powerdomain *pwrdm);
int pwrdm_clkdm_state_switch(struct clockdomain *clkdm);
int pwrdm_pre_transition(struct powerdomain *pwrdm);
int pwrdm_post_transition(struct powerdomain *pwrdm);
int pwrdm_usecount_inc(struct powerdomain *pwrdm);
int pwrdm_usecount_dec(struct powerdomain *pwrdm);
int pwrdm_get_usecount(struct powerdomain *pwrdm);
int pwrdm_set_lowpwrstchange(struct powerdomain *pwrdm);

int pwrdm_wakeuplat_update_constraint(struct powerdomain *pwrdm, void *cookie,
				      long min_latency);
int pwrdm_wakeuplat_remove_constraint(struct powerdomain *pwrdm, void *cookie);

int pwrdm_get_context_loss_count(struct powerdomain *pwrdm);
bool pwrdm_can_ever_lose_context(struct powerdomain *pwrdm);
int pwrdm_enable_force_off(struct powerdomain *pwrdm);
int pwrdm_disable_force_off(struct powerdomain *pwrdm);

int omap_set_pwrdm_state(struct powerdomain *pwrdm, u32 state);
extern void omap242x_powerdomains_init(void);
extern void omap243x_powerdomains_init(void);
extern void omap3xxx_powerdomains_init(void);
extern void omap44xx_powerdomains_init(void);
extern void omap54xx_powerdomains_init(void);

extern struct pwrdm_ops omap2_pwrdm_operations;
extern struct pwrdm_ops omap3_pwrdm_operations;
extern struct pwrdm_ops omap4_pwrdm_operations;
extern struct pwrdm_ops omap5_pwrdm_operations;

/* Common Internal functions used across OMAP rev's */
extern u32 omap2_pwrdm_get_mem_bank_onstate_mask(u8 bank);
extern u32 omap2_pwrdm_get_mem_bank_retst_mask(u8 bank);
extern u32 omap2_pwrdm_get_mem_bank_stst_mask(u8 bank);

extern struct powerdomain wkup_omap2_pwrdm;
extern struct powerdomain gfx_omap2_pwrdm;

#define PWRDM_COMPARE_PWRST_EQ	(0x1 << 0)
#define PWRDM_COMPARE_PWRST_GT	(0x1 << 1)
#define PWRDM_COMPARE_PWRST_LT	(0x1 << 2)
/* Do not use the internal function, use the exposed API */
extern bool _pwrdm_state_compare_int(int a, int b, u8 flag);

/*
 * The following inlines are strongly encouraged to be
 * used in-order to compare powerdomain states.
 * pwrdm_power_state_eq(a, b) return true if a == b
 * pwrdm_power_state_gt(a, b) return true if a > b
 * pwrdm_power_state_ge(a, b) return true if a >= b
 * pwrdm_power_state_lt(a, b) return true if a < b
 * pwrdm_power_state_le(a, b) return true if a <= b
 *
 * IMPORTANT: For performance reasons,No verification of powerdomains are done
 * Do not use with private macro definition of internal state
 */
static inline bool pwrdm_power_state_eq(int a, int b)
{
	return _pwrdm_state_compare_int(a, b, PWRDM_COMPARE_PWRST_EQ);
}
static inline bool pwrdm_power_state_gt(int a, int b)
{
	return _pwrdm_state_compare_int(a, b, PWRDM_COMPARE_PWRST_GT);
}
static inline bool pwrdm_power_state_ge(int a, int b)
{
	return _pwrdm_state_compare_int(a, b,
					PWRDM_COMPARE_PWRST_GT |
					PWRDM_COMPARE_PWRST_EQ);
}
static inline bool pwrdm_power_state_lt(int a, int b)
{
	return _pwrdm_state_compare_int(a, b, PWRDM_COMPARE_PWRST_LT);
}

static inline bool pwrdm_power_state_le(int a, int b)
{
	return _pwrdm_state_compare_int(a, b,
					PWRDM_COMPARE_PWRST_LT |
					PWRDM_COMPARE_PWRST_EQ);
}
#endif
