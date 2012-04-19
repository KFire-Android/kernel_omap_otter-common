/*
 * Duty sycle governor
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Eugene Mandrenko <ievgen.mandrenko@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef __OMAP4_DUTY_CYCLE_GOVERNOR_H__
#define __OMAP4_DUTY_CYCLE_GOVERNOR_H__

struct pcb_sens {
	int (*update_temp) (void);
};

struct duty_cycle_params {
	u32 nitro_rate;		/* the maximum OPP frequency */
	u32 cooling_rate;	/* the OPP used to cool off */
	u32 nitro_interval;	/* time interval to control the duty cycle */
	u32 nitro_percentage;	/* % out of nitro_interval to use max OPP */
};

struct pcb_section {
	u32 pcb_temp_level;
	u32 max_opp;
	struct duty_cycle_params tduty_params;
	bool duty_cycle_enabled;
};

struct duty_cycle {
	int (*update_params)(struct duty_cycle_params *);
	int (*enable)(bool val, bool update);
};

void omap4_duty_pcb_section_reg(struct pcb_section *pcb_sect, int sect_size);
int omap4_duty_pcb_register(struct pcb_sens *tpcb);

#ifdef CONFIG_OMAP4_DUTY_CYCLE_GOVERNOR
int omap4_duty_cycle_register(struct duty_cycle *tduty);
#else
static inline int omap4_duty_cycle_register(struct duty_cycle *tduty)
{
	return 0;
}
#endif


#endif /*__OMAP4_DUTY_CYCLE_GOVERNOR_H__*/
