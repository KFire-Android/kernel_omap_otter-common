/*
 * Duty cycle governor
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Eugene Mandrenko <Ievgen.mandrenko@ti.com>
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/omap4_duty_cycle_governor.h>
#include <plat/omap_device.h>

#define NORMAL_TEMP_MONITORING_RATE 1000
#define NORMAL_MONITORING_RATE 10000
#define DEFAULT_TEMPERATURE 65000
#define TEMP_THRESHOLD 1
#define INIT_SECTION -1

struct duty_governor {
	struct pcb_sens *tpcb;
	struct duty_cycle *tduty;
	struct pcb_section *tpcb_sections;
	int period;
	int previous_temp;
	int curr_pcb_temp;
	int previous_pcb_temp;
	int working_section;
	int npcb_sections;
	struct delayed_work duty_cycle_governor_work;
};

static struct duty_governor *t_governor;
static struct pcb_section *pcb_sections;
static int pcb_sections_size;

void omap4_duty_pcb_section_reg(struct pcb_section *pcb_sect, int sect_size)
{
	pcb_sections = pcb_sect;
	pcb_sections_size = sect_size;
}

static void omap4_duty_schedule(struct duty_governor *t_gov)
{
	if (!IS_ERR_OR_NULL(t_gov) &&
	    !IS_ERR_OR_NULL(t_gov->tpcb) &&
	    !IS_ERR_OR_NULL(t_gov->tduty))
		schedule_delayed_work(&t_governor->duty_cycle_governor_work,
				msecs_to_jiffies(0));
}

int omap4_duty_pcb_register(struct pcb_sens *tpcb)
{
	if (!IS_ERR_OR_NULL(t_governor)) {
		if (t_governor->tpcb == NULL) {
			t_governor->tpcb = tpcb;
			t_governor->period = NORMAL_TEMP_MONITORING_RATE;
		} else {
			pr_err("%s:dublicate of pcb registration\n", __func__);

			return -EBUSY;
		}
	}
	omap4_duty_schedule(t_governor);

	return 0;
}
static bool is_treshold(struct duty_governor *tgov)
{
	int delta;

	delta = abs(tgov->previous_pcb_temp - tgov->curr_pcb_temp);

	if (delta > TEMP_THRESHOLD)
		return true;

	return false;
}

static int omap4_duty_apply_constraint(struct duty_governor *tgov,
					int sect_num)
{
	struct pcb_section *t_pcb_sections = &tgov->tpcb_sections[sect_num];
	struct duty_cycle_params *tduty_params = &t_pcb_sections->tduty_params;
	int dc_enabled = t_pcb_sections->duty_cycle_enabled;
	struct duty_cycle *t_duty = tgov->tduty;
	int ret = true;

	if (tgov->working_section != sect_num) {
		ret = tgov->tduty->enable(false, false);

		if (ret)
			return ret;

		if (dc_enabled) {
			if (t_duty->update_params(tduty_params))
				return ret;

			tgov->tduty->enable(dc_enabled, true);
		}
		tgov->working_section = sect_num;
	}

	return ret;
}

static void omap4_duty_update(struct duty_governor *tgov)
{
	int sect_num;

	for (sect_num = 0; sect_num < tgov->npcb_sections; sect_num++)
		if (tgov->tpcb_sections[sect_num].pcb_temp_level >
				tgov->curr_pcb_temp)
			break;
	if (sect_num >= tgov->npcb_sections)
		sect_num = tgov->npcb_sections - 1;

	if (omap4_duty_apply_constraint(tgov, sect_num))
		tgov->previous_pcb_temp = tgov->curr_pcb_temp;
}

static void omap4_duty_governor_delayed_work_fn(struct work_struct *work)
{
	if (!IS_ERR_OR_NULL(t_governor->tpcb)) {
		if (!IS_ERR_OR_NULL(t_governor->tpcb->update_temp)) {
			t_governor->curr_pcb_temp =
					t_governor->tpcb->update_temp();
			if (is_treshold(t_governor))
				omap4_duty_update(t_governor);
		} else {
			pr_err("%s:update_temp() isn't defined\n", __func__);
		}
	}
	schedule_delayed_work(&t_governor->duty_cycle_governor_work,
		msecs_to_jiffies(t_governor->period));
}

int omap4_duty_cycle_register(struct duty_cycle *tduty)
{
	if (!IS_ERR_OR_NULL(t_governor)) {
		if (t_governor->tduty == NULL) {
			t_governor->tduty = tduty;
		} else {
			pr_err("%s:dublicate of duty cycle registration\n",
						__func__);

			return -EBUSY;
		}
	}
	omap4_duty_schedule(t_governor);

	return 0;
}

static int __init omap4_duty_governor_init(void)
{
	if (!cpu_is_omap443x())
		return 0;

	t_governor = kzalloc(sizeof(struct duty_governor), GFP_KERNEL);
	if (IS_ERR_OR_NULL(t_governor)) {
		pr_err("%s:Cannot allocate memory\n", __func__);

		return -ENOMEM;
	}
	t_governor->period = NORMAL_MONITORING_RATE;
	t_governor->previous_temp = DEFAULT_TEMPERATURE;
	t_governor->tpcb_sections = pcb_sections;
	t_governor->npcb_sections = pcb_sections_size;
	t_governor->working_section = INIT_SECTION;
	INIT_DELAYED_WORK(&t_governor->duty_cycle_governor_work,
				omap4_duty_governor_delayed_work_fn);

	return 0;
}

static void __exit omap4_duty_governor_exit(void)
{
	cancel_delayed_work_sync(&t_governor->duty_cycle_governor_work);
	kfree(t_governor);
}

early_initcall(omap4_duty_governor_init);
module_exit(omap4_duty_governor_exit);

MODULE_AUTHOR("Euvgen Mandrenko <ievgen.mandrenko@ti.com>");
MODULE_DESCRIPTION("OMAP on-die thermal governor");
MODULE_LICENSE("GPL");

