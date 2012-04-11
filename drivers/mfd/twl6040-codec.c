/*
 * MFD driver for twl6040 codec submodule
 *
 * Authors:     Jorge Eduardo Candelaria <jorge.candelaria@ti.com>
 *              Misael Lopez Cruz <misael.lopez@ti.com>
 *
 * Copyright:   (C) 20010 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/core.h>
#include <linux/mfd/twl6040-codec.h>

static int twl6040_power(struct twl6040 *twl6040, int enable);

int twl6040_get_reg_supply(unsigned int reg)
{
	if (reg > TWL6040_CACHEREGNUM)
		return -EINVAL;

	switch (reg) {
	case TWL6040_REG_ASICID:
	case TWL6040_REG_ASICREV:
	case TWL6040_REG_INTID:
	case TWL6040_REG_INTMR:
	case TWL6040_REG_NCPCTL:
	case TWL6040_REG_LDOCTL:
	case TWL6040_REG_AMICBCTL:
	case TWL6040_REG_DMICBCTL:
	case TWL6040_REG_HKCTL1:
	case TWL6040_REG_HKCTL2:
	case TWL6040_REG_GPOCTL:
	case TWL6040_REG_TRIM1:
	case TWL6040_REG_TRIM2:
	case TWL6040_REG_TRIM3:
	case TWL6040_REG_HSOTRIM:
	case TWL6040_REG_HFOTRIM:
	case TWL6040_REG_ACCCTL:
	case TWL6040_REG_STATUS:
		return TWL6040_VIO_SUPPLY;
	default:
		return TWL6040_VDD_SUPPLY;
	}
}
EXPORT_SYMBOL(twl6040_get_reg_supply);

int twl6040_reg_is_vdd(unsigned int reg)
{
	return twl6040_get_reg_supply(reg) == TWL6040_VDD_SUPPLY;
}
EXPORT_SYMBOL(twl6040_reg_is_vdd);

int twl6040_reg_is_vio(unsigned int reg)
{
	return twl6040_get_reg_supply(reg) == TWL6040_VIO_SUPPLY;
}
EXPORT_SYMBOL(twl6040_reg_is_vio);

static inline int twl6040_cache_read(struct twl6040 *twl6040, unsigned int reg)
{
	return twl6040->cache[reg];
}

/* twl6040->io_mutex must already be locked when calling this function */
static int twl6040_i2c_read(struct twl6040 *twl6040, unsigned int reg)
{
	int ret;
	u8 val = 0;

	ret = twl_i2c_read_u8(TWL_MODULE_AUDIO_VOICE, &val, reg);
	if (ret)
		return ret;

	return val;
}

int twl6040_reg_read(struct twl6040 *twl6040, unsigned int reg)
{
	int ret;

	mutex_lock(&twl6040->io_mutex);
	if (twl6040_reg_is_vio(reg) || likely(!twl6040->thshut)) {
		ret = twl6040_i2c_read(twl6040, reg);
	} else {
		dev_warn(twl6040->dev,
			"reg 0x%02x read from cache (thermal shutdown)\n",
			reg);
		ret = twl6040_cache_read(twl6040, reg);
	}
	mutex_unlock(&twl6040->io_mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_reg_read);

static inline int twl6040_cache_write(struct twl6040 *twl6040,
				unsigned int reg, u8 val)
{
	twl6040->cache[reg] = val;
	return 0;
}

/* twl6040->io_mutex must already be locked when calling this function */
static int twl6040_i2c_write(struct twl6040 *twl6040, unsigned int reg, u8 val)
{
	int ret;

	ret = twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, val, reg);
	if (ret)
		return ret;

	twl6040_cache_write(twl6040, reg, val);
	return 0;
}

int twl6040_reg_write(struct twl6040 *twl6040, unsigned int reg, u8 val)
{
	int ret;

	mutex_lock(&twl6040->io_mutex);
	if (twl6040_reg_is_vio(reg) || likely(!twl6040->thshut)) {
		ret = twl6040_i2c_write(twl6040, reg, val);
	} else {
		dev_warn(twl6040->dev,
			"reg 0x%02x write to cache (thermal shutdown)\n",
			reg);
		ret = twl6040_cache_write(twl6040, reg, val);
	}
	mutex_unlock(&twl6040->io_mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_reg_write);

static inline int twl6040_cache_set_bits(struct twl6040 *twl6040,
				unsigned int reg, u8 mask)
{
	twl6040->cache[reg] = twl6040->cache[reg] | mask;
	return 0;
}

/* twl6040->io_mutex must already be locked when calling this function */
static int twl6040_i2c_set_bits(struct twl6040 *twl6040,
				unsigned int reg, u8 mask)
{
	int ret;
	u8 val = 0;

	ret = twl_i2c_read_u8(TWL_MODULE_AUDIO_VOICE, &val, reg);
	if (ret)
		return ret;

	val |= mask;
	ret = twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, val, reg);
	if (ret)
		return ret;

	twl6040_cache_write(twl6040, reg, val);
	return 0;
}

int twl6040_set_bits(struct twl6040 *twl6040, unsigned int reg, u8 mask)
{
	int ret;

	mutex_lock(&twl6040->io_mutex);
	if (twl6040_reg_is_vio(reg) || likely(!twl6040->thshut)) {
		ret = twl6040_i2c_set_bits(twl6040, reg, mask);
	} else {
		dev_warn(twl6040->dev,
			"reg 0x%02x access to cache (thermal shutdown)\n",
			reg);
		ret = twl6040_cache_set_bits(twl6040, reg, mask);
	}
	mutex_unlock(&twl6040->io_mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_set_bits);

static inline int twl6040_cache_clear_bits(struct twl6040 *twl6040,
				unsigned int reg, u8 mask)
{
	twl6040->cache[reg] = twl6040->cache[reg] & ~mask;
	return 0;
}

/* twl6040->io_mutex must already be locked when calling this function */
static int twl6040_i2c_clear_bits(struct twl6040 *twl6040,
				unsigned int reg, u8 mask)
{
	int ret;
	u8 val = 0;

	ret = twl_i2c_read_u8(TWL_MODULE_AUDIO_VOICE, &val, reg);
	if (ret)
		return ret;

	val &= ~mask;
	ret = twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, val, reg);
	if (ret)
		return ret;

	twl6040_cache_write(twl6040, reg, val);
	return 0;
}

int twl6040_clear_bits(struct twl6040 *twl6040, unsigned int reg, u8 mask)
{
	int ret;

	mutex_lock(&twl6040->io_mutex);
	if (twl6040_reg_is_vio(reg) || likely(!twl6040->thshut)) {
		ret = twl6040_i2c_clear_bits(twl6040, reg, mask);
	} else {
		dev_warn(twl6040->dev,
			"reg 0x%02x access to cache (thermal shutdown)\n",
			reg);
		ret = twl6040_cache_clear_bits(twl6040, reg, mask);
	}
	mutex_unlock(&twl6040->io_mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_clear_bits);

/* Restore context of VDD/VSS or VIO/GND registers */
static int twl6040_restore_ctx(struct twl6040 *twl6040, int supply)
{
	int reg, ret = 0;

	for (reg = 1; reg < TWL6040_CACHEREGNUM; reg++) {
		/* skip read-only registers */
		switch (reg) {
		case TWL6040_REG_ASICID:
		case TWL6040_REG_ASICREV:
		case TWL6040_REG_UPCRES:
		case TWL6040_REG_DNCRES:
			continue;
		default:
			break;
		}

		if (twl6040_get_reg_supply(reg) == supply) {
			ret = twl6040_reg_write(twl6040, reg,
						twl6040_cache_read(twl6040, reg));
			if (ret) {
				dev_err(twl6040->dev,
					"failed to restore context %d\n", ret);
				break;
			}
		}
	}

	return ret;
}

/* twl6040 codec manual power-up sequence */
static int twl6040_power_up(struct twl6040 *twl6040)
{
	u8 ncpctl, ldoctl, lppllctl, accctl;
	int ret;

	ncpctl = twl6040_reg_read(twl6040, TWL6040_REG_NCPCTL);
	ldoctl = twl6040_reg_read(twl6040, TWL6040_REG_LDOCTL);
	lppllctl = twl6040_reg_read(twl6040, TWL6040_REG_LPPLLCTL);
	accctl = twl6040_reg_read(twl6040, TWL6040_REG_ACCCTL);

	/* enable reference system */
	ldoctl |= TWL6040_REFENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		return ret;
	msleep(10);

	/* enable internal oscillator */
	ldoctl |= TWL6040_OSCENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto osc_err;
	udelay(10);

	/* enable high-side ldo */
	ldoctl |= TWL6040_HSLDOENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto hsldo_err;
	udelay(244);

	/* enable negative charge pump */
	ncpctl |= TWL6040_NCPENA | TWL6040_NCPOPEN;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_NCPCTL, ncpctl);
	if (ret)
		goto ncp_err;
	udelay(488);

	/* enable low-side ldo */
	ldoctl |= TWL6040_LSLDOENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto lsldo_err;
	udelay(244);

	/* enable low-power pll */
	lppllctl |= TWL6040_LPLLENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);
	if (ret)
		goto lppll_err;

	/* reset state machine */
	accctl |= TWL6040_RESETSPLIT;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_ACCCTL, accctl);
	if (ret)
		goto rst_err;
	mdelay(5);
	accctl &= ~TWL6040_RESETSPLIT;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_ACCCTL, accctl);
	if (ret)
		goto rst_err;

	/* disable internal oscillator */
	ldoctl &= ~TWL6040_OSCENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto rst_err;

	return 0;

rst_err:
	lppllctl &= ~TWL6040_LPLLENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);
lppll_err:
	ldoctl &= ~TWL6040_LSLDOENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	udelay(244);
lsldo_err:
	ncpctl &= ~(TWL6040_NCPENA | TWL6040_NCPOPEN);
	twl6040_reg_write(twl6040, TWL6040_REG_NCPCTL, ncpctl);
	udelay(488);
ncp_err:
	ldoctl &= ~TWL6040_HSLDOENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	udelay(244);
hsldo_err:
	ldoctl &= ~TWL6040_OSCENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
osc_err:
	ldoctl &= ~TWL6040_REFENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	msleep(10);

	return ret;
}

/* twl6040 codec manual power-down sequence */
static int twl6040_power_down(struct twl6040 *twl6040)
{
	u8 ncpctl, ldoctl, lppllctl, accctl;
	int ret;

	ncpctl = twl6040_reg_read(twl6040, TWL6040_REG_NCPCTL);
	ldoctl = twl6040_reg_read(twl6040, TWL6040_REG_LDOCTL);
	lppllctl = twl6040_reg_read(twl6040, TWL6040_REG_LPPLLCTL);
	accctl = twl6040_reg_read(twl6040, TWL6040_REG_ACCCTL);

	/* enable internal oscillator */
	ldoctl |= TWL6040_OSCENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		return ret;
	udelay(10);

	/* disable low-power pll */
	lppllctl &= ~TWL6040_LPLLENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);
	if (ret)
		goto lppll_err;

	/* disable low-side ldo */
	ldoctl &= ~TWL6040_LSLDOENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto lsldo_err;
	udelay(244);

	/* disable negative charge pump */
	ncpctl &= ~(TWL6040_NCPENA | TWL6040_NCPOPEN);
	ret = twl6040_reg_write(twl6040, TWL6040_REG_NCPCTL, ncpctl);
	if (ret)
		goto ncp_err;
	udelay(488);

	/* disable high-side ldo */
	ldoctl &= ~TWL6040_HSLDOENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto hsldo_err;
	udelay(244);

	/* disable internal oscillator */
	ldoctl &= ~TWL6040_OSCENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto osc_err;

	/* disable reference system */
	ldoctl &= ~TWL6040_REFENA;
	ret = twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	if (ret)
		goto ref_err;
	msleep(10);

	return 0;

ref_err:
	ldoctl |= TWL6040_OSCENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	udelay(10);
osc_err:
	ldoctl |= TWL6040_HSLDOENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	udelay(244);
hsldo_err:
	ncpctl |= TWL6040_NCPENA | TWL6040_NCPOPEN;
	twl6040_reg_write(twl6040, TWL6040_REG_NCPCTL, ncpctl);
	udelay(488);
ncp_err:
	ldoctl |= TWL6040_LSLDOENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	udelay(244);
lsldo_err:
	lppllctl |= TWL6040_LPLLENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);
lppll_err:
	lppllctl |= TWL6040_LPLLENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);
	accctl |= TWL6040_RESETSPLIT;
	twl6040_reg_write(twl6040, TWL6040_REG_ACCCTL, accctl);
	mdelay(5);
	accctl &= ~TWL6040_RESETSPLIT;
	twl6040_reg_write(twl6040, TWL6040_REG_ACCCTL, accctl);
	ldoctl &= ~TWL6040_OSCENA;
	twl6040_reg_write(twl6040, TWL6040_REG_LDOCTL, ldoctl);
	msleep(10);

	return ret;
}

static irqreturn_t twl6040_naudint_handler(int irq, void *data)
{
	struct twl6040 *twl6040 = data;
	u8 intid, val;

	intid = twl6040_reg_read(twl6040, TWL6040_REG_INTID);

	if (intid & TWL6040_READYINT)
		complete(&twl6040->ready);

	if (intid & TWL6040_THINT) {
		val = twl6040_reg_read(twl6040, TWL6040_REG_STATUS);
		if (val & TWL6040_TSHUTDET) {
			dev_err(twl6040->dev,
				"thermal shutdown started\n");

			twl6040->thshut = 1;

			/*
			 * power-down during thermal event keeps reference
			 * system and temperature sensor active
			 */
			mutex_lock(&twl6040->mutex);
			twl6040_power(twl6040, 0);
			mutex_unlock(&twl6040->mutex);

			twl6040_report_event(twl6040, TWL6040_THSHUT_EVENT);
		} else {
			dev_info(twl6040->dev,
				"recovered from thermal shutdown\n");

			twl6040->thshut = 0;

			/*
			 * power-up after recovered from thermal event can
			 * be followed by power-down in case MFD is not
			 * enabled so that reference system can be disabled
			 */
			mutex_lock(&twl6040->mutex);
			twl6040_power(twl6040, 1);
			if (!twl6040->power_count)
				twl6040_power(twl6040, 0);
			else
				twl6040_restore_ctx(twl6040,
						TWL6040_VDD_SUPPLY);
			mutex_unlock(&twl6040->mutex);

			twl6040_report_event(twl6040, TWL6040_THSHUT_RECOVERY);
		}
	}

	return IRQ_HANDLED;
}

static int twl6040_is_powered(struct twl6040 *twl6040)
{
	int ncpctl;
	int ldoctl;
	int lppllctl;
	u8 ncpctl_exp;
	u8 ldoctl_exp;
	u8 lppllctl_exp;

	/* NCPCTL expected value: NCP enabled */
	ncpctl_exp = (TWL6040_TSHUTENA | TWL6040_NCPENA);

	/* LDOCTL expected value: HS/LS LDOs and Reference enabled */
	ldoctl_exp = (TWL6040_REFENA | TWL6040_HSLDOENA | TWL6040_LSLDOENA);

	/* LPPLLCTL expected value: Low-Power PLL enabled */
	lppllctl_exp = TWL6040_LPLLENA;

	ncpctl = twl6040_reg_read(twl6040, TWL6040_REG_NCPCTL);
	if (ncpctl < 0)
		return 0;

	ldoctl = twl6040_reg_read(twl6040, TWL6040_REG_LDOCTL);
	if (ldoctl < 0)
		return 0;

	lppllctl = twl6040_reg_read(twl6040, TWL6040_REG_LPPLLCTL);
	if (lppllctl < 0)
		return 0;

	if ((ncpctl != ncpctl_exp) ||
	    (ldoctl != ldoctl_exp) ||
	    (lppllctl != lppllctl_exp)) {
		dev_warn(twl6040->dev,
			"NCPCTL: 0x%02x (should be 0x%02x)\n"
			"LDOCTL: 0x%02x (should be 0x%02x)\n"
			"LPLLCTL: 0x%02x (should be 0x%02x)\n",
			ncpctl, ncpctl_exp,
			ldoctl, ldoctl_exp,
			lppllctl, lppllctl_exp);
		return 0;
	}

	return 1;
}

static int twl6040_power_up_completion(struct twl6040 *twl6040,
				       int naudint)
{
	int time_left;
	int round = 0;
	int ret = 0;
	u8 intid;

	do {
		INIT_COMPLETION(twl6040->ready);
		gpio_set_value(twl6040->audpwron, 1);
		time_left = wait_for_completion_timeout(&twl6040->ready,
							msecs_to_jiffies(700));

		if (twl6040_is_powered(twl6040))
			return 0;

		if (!time_left) {
			intid = twl6040_reg_read(twl6040, TWL6040_REG_INTID);
			if (!(intid & TWL6040_READYINT)) {
				dev_err(twl6040->dev,
					"timeout waiting for READYINT\n");
				return -ETIMEDOUT;
			}
		}

		/*
		 * Power on seemingly completed.
		 * READYINT received, but not in expected state, retry.
		 */
		gpio_set_value(twl6040->audpwron, 0);
		usleep_range(1000, 1500);
	} while (round++ < 3);

	if (round >= 3) {
		dev_err(twl6040->dev,
			"Automatic power on failed, reverting to manual\n");
		twl6040->audpwron = -EINVAL;
		ret = twl6040_power_up(twl6040);
		if (ret)
			dev_err(twl6040->dev, "Manual power-up failed\n");
	}

	return ret;
}

static int twl6040_power(struct twl6040 *twl6040, int enable)
{
	struct twl4030_codec_data *pdata = dev_get_platdata(twl6040->dev);
	int audpwron = twl6040->audpwron;
	int naudint = twl6040->irq;
	int ret = 0;

	if (enable) {
		/* enable 32kHz external clock */
		if (pdata->set_ext_clk32k) {
			ret = pdata->set_ext_clk32k(true);
			if (ret) {
				dev_err(twl6040->dev,
					"failed to enable CLK32K %d\n", ret);
				return ret;
			}
		}

		/* disable internal 32kHz oscillator */
		twl6040_clear_bits(twl6040, TWL6040_REG_ACCCTL,
				TWL6040_CLK32KSEL);

		if (gpio_is_valid(audpwron)) {
			/* wait for power-up completion */
			ret = twl6040_power_up_completion(twl6040, naudint);
			if (ret) {
				dev_err(twl6040->dev,
					"automatic power-down failed\n");
				return ret;
			}
		} else {
			/* use manual power-up sequence */
			ret = twl6040_power_up(twl6040);
			if (ret) {
				dev_err(twl6040->dev,
					"manual power-up failed\n");
				return ret;
			}
		}

		/* Errata: PDMCLK can fail to generate at cold temperatures
		 * The workaround consists of resetting HPPLL and LPPLL
		 * after Sleep/Deep-Sleep mode and before application mode.
		 */
		twl6040_set_bits(twl6040, TWL6040_REG_HPPLLCTL,
				TWL6040_HPLLRST);
		twl6040_clear_bits(twl6040, TWL6040_REG_HPPLLCTL,
				TWL6040_HPLLRST);
		twl6040_set_bits(twl6040, TWL6040_REG_LPPLLCTL,
				TWL6040_LPLLRST);
		twl6040_clear_bits(twl6040, TWL6040_REG_LPPLLCTL,
				TWL6040_LPLLRST);

		twl6040->pll = TWL6040_LPPLL_ID;
		twl6040->sysclk = 19200000;
	} else {
		if (gpio_is_valid(audpwron)) {
			/* use AUDPWRON line */
			gpio_set_value(audpwron, 0);

			/* power-down sequence latency */
			udelay(500);
		} else {
			/* use manual power-down sequence */
			ret = twl6040_power_down(twl6040);
			if (ret) {
				dev_err(twl6040->dev,
					"manual power-down failed\n");
				return ret;
			}
		}

		/* enable internal 32kHz oscillator */
		twl6040_set_bits(twl6040, TWL6040_REG_ACCCTL,
				TWL6040_CLK32KSEL);

		/* disable 32kHz external clock */
		if (pdata->set_ext_clk32k) {
			ret = pdata->set_ext_clk32k(false);
			if (ret)
				dev_err(twl6040->dev,
					"failed to disable CLK32K %d\n", ret);
		}

		twl6040->pll = TWL6040_NOPLL_ID;
		twl6040->sysclk = 0;
	}

	twl6040->powered = enable;

	return ret;
}

int twl6040_enable(struct twl6040 *twl6040)
{
	int ret = 0;

	if (twl6040->thshut) {
		dev_warn(twl6040->dev,
			"Power-up denied while in thermal shutdown\n");
		return -EPERM;
	}

	mutex_lock(&twl6040->mutex);
	if (!twl6040->power_count++)
		ret = twl6040_power(twl6040, 1);
	mutex_unlock(&twl6040->mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_enable);

int twl6040_disable(struct twl6040 *twl6040)
{
	int ret = 0;

	mutex_lock(&twl6040->mutex);
	WARN(!twl6040->power_count, "TWL6040 is already disabled");
	if (!--twl6040->power_count)
		ret = twl6040_power(twl6040, 0);
	mutex_unlock(&twl6040->mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_disable);

int twl6040_is_enabled(struct twl6040 *twl6040)
{
	return twl6040->power_count;
}
EXPORT_SYMBOL(twl6040_is_enabled);

int twl6040_set_pll(struct twl6040 *twl6040, enum twl6040_pll_id id,
		    unsigned int freq_in, unsigned int freq_out)
{
	u8 hppllctl, lppllctl;
	int ret = 0;

	mutex_lock(&twl6040->mutex);

	hppllctl = twl6040_reg_read(twl6040, TWL6040_REG_HPPLLCTL);
	lppllctl = twl6040_reg_read(twl6040, TWL6040_REG_LPPLLCTL);

	switch (id) {
	case TWL6040_LPPLL_ID:
		/* lppll divider */
		switch (freq_out) {
		case 17640000:
			lppllctl |= TWL6040_LPLLFIN;
			break;
		case 19200000:
			lppllctl &= ~TWL6040_LPLLFIN;
			break;
		default:
			dev_err(twl6040->dev,
				"freq_out %d not supported\n", freq_out);
			ret = -EINVAL;
			goto pll_out;
		}
		twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);

		switch (freq_in) {
		case 32768:
			lppllctl |= TWL6040_LPLLENA;
			twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL,
					  lppllctl);
			mdelay(5);
			lppllctl &= ~TWL6040_HPLLSEL;
			twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL,
					  lppllctl);
			hppllctl &= ~TWL6040_HPLLENA;
			twl6040_reg_write(twl6040, TWL6040_REG_HPPLLCTL,
					  hppllctl);
			break;
		default:
			dev_err(twl6040->dev,
				"freq_in %d not supported\n", freq_in);
			ret = -EINVAL;
			goto pll_out;
		}

		twl6040->pll = TWL6040_LPPLL_ID;
		break;
	case TWL6040_HPPLL_ID:
		/* high-performance pll can provide only 19.2 MHz */
		if (freq_out != 19200000) {
			dev_err(twl6040->dev,
				"freq_out %d not supported\n", freq_out);
			ret = -EINVAL;
			goto pll_out;
		}

		hppllctl &= ~TWL6040_MCLK_MSK;

		switch (freq_in) {
		case 12000000:
			/* mclk input, pll enabled */
			hppllctl |= TWL6040_MCLK_12000KHZ |
				    TWL6040_HPLLSQRBP |
				    TWL6040_HPLLENA;
			break;
		case 19200000:
			/* mclk input, pll disabled */
			hppllctl |= TWL6040_MCLK_19200KHZ |
				    TWL6040_HPLLSQRENA |
				    TWL6040_HPLLBP;
			break;
		case 26000000:
			/* mclk input, pll enabled */
			hppllctl |= TWL6040_MCLK_26000KHZ |
				    TWL6040_HPLLSQRBP |
				    TWL6040_HPLLENA;
			break;
		case 38400000:
			/* clk slicer, pll disabled */
			hppllctl |= TWL6040_MCLK_38400KHZ |
				    TWL6040_HPLLSQRENA |
				    TWL6040_HPLLBP;
			break;
		default:
			dev_err(twl6040->dev,
				"freq_in %d not supported\n", freq_in);
			ret = -EINVAL;
			goto pll_out;
		}

		twl6040_reg_write(twl6040, TWL6040_REG_HPPLLCTL, hppllctl);
		udelay(500);
		lppllctl |= TWL6040_HPLLSEL;
		twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);
		lppllctl &= ~TWL6040_LPLLENA;
		twl6040_reg_write(twl6040, TWL6040_REG_LPPLLCTL, lppllctl);

		twl6040->pll = TWL6040_HPPLL_ID;
		break;
	default:
		dev_err(twl6040->dev, "unknown pll id %d\n", id);
		ret = -EINVAL;
		goto pll_out;
	}

	twl6040->sysclk = freq_out;

pll_out:
	mutex_unlock(&twl6040->mutex);
	return ret;
}
EXPORT_SYMBOL(twl6040_set_pll);

enum twl6040_pll_id twl6040_get_pll(struct twl6040 *twl6040)
{
	return twl6040->pll;
}
EXPORT_SYMBOL(twl6040_get_pll);

unsigned int twl6040_get_sysclk(struct twl6040 *twl6040)
{
	return twl6040->sysclk;
}
EXPORT_SYMBOL(twl6040_get_sysclk);

int twl6040_get_icrev(struct twl6040 *twl6040)
{
	return twl6040->icrev;
}
EXPORT_SYMBOL(twl6040_get_icrev);

#define EVENT_NAME "TWL6040_EVENT"

void twl6040_report_event(struct twl6040 *twl6040, int event)
{
	char name_buf[120];
	char state_buf[120];
	char *envp[3];
	int env_offset = 0;

	switch (event) {
	case TWL6040_THSHUT_EVENT:
	case TWL6040_THSHUT_RECOVERY:
	case TWL6040_HFOC_EVENT:
	case TWL6040_VIBOC_EVENT:
		snprintf(name_buf, sizeof(name_buf),
			"EVENT_NAME=%s", EVENT_NAME);
		envp[env_offset++] = name_buf;

		snprintf(state_buf, sizeof(state_buf),
			"EVENT_INDEX=%d", event);
		envp[env_offset++] = state_buf;

		envp[env_offset] = NULL;
		kobject_uevent_env(&twl6040->dev->kobj, KOBJ_CHANGE, envp);
		break;
	default:
		dev_err(twl6040->dev, "event not supported %d\n", event);
		break;
	}
}
EXPORT_SYMBOL(twl6040_report_event);

static int __devinit twl6040_probe(struct platform_device *pdev)
{
	struct twl4030_codec_data *pdata = pdev->dev.platform_data;
	struct twl6040 *twl6040;
	struct mfd_cell *cell = NULL;
	unsigned int naudint;
	int audpwron;
	int ret, children = 0;
	u8 accctl;

	if(!pdata) {
		dev_err(&pdev->dev, "Platform data is missing\n");
		return -EINVAL;
	}

	twl6040 = kzalloc(sizeof(struct twl6040), GFP_KERNEL);
	if (!twl6040)
		return -ENOMEM;

	platform_set_drvdata(pdev, twl6040);

	twl6040->dev = &pdev->dev;
	mutex_init(&twl6040->mutex);
	mutex_init(&twl6040->io_mutex);

	if (pdata->init) {
		ret = pdata->init();
		if (ret) {
			dev_err(twl6040->dev, "Platform init failed %d\n",
				ret);
			goto init_err;
		}
	}

	twl6040->icrev = twl6040_reg_read(twl6040, TWL6040_REG_ASICREV);
	if (twl6040->icrev < 0) {
		ret = twl6040->icrev;
		goto rev_err;
	}

	if (pdata && (twl6040_get_icrev(twl6040) > TWL6040_REV_1_0))
		audpwron = pdata->audpwron_gpio;
	else
		audpwron = -EINVAL;

	if (pdata)
		naudint = pdata->naudint_irq;
	else
		naudint = 0;

	twl6040->audpwron = audpwron;
	twl6040->powered = 0;
	twl6040->irq = naudint;
	twl6040->irq_base = pdata->irq_base;
	init_completion(&twl6040->ready);

	if (gpio_is_valid(audpwron)) {
		ret = gpio_request(audpwron, "audpwron");
		if (ret)
			goto rev_err;

		ret = gpio_direction_output(audpwron, 0);
		if (ret)
			goto gpio2_err;
	}

	if (naudint) {
		/* codec interrupt */
		ret = twl6040_irq_init(twl6040);
		if (ret)
			goto gpio2_err;

		ret = twl6040_request_irq(twl6040, TWL6040_IRQ_READY,
				twl6040_naudint_handler, 0,
				"twl6040_irq_ready", twl6040);
		if (ret) {
			dev_err(twl6040->dev, "READY IRQ request failed: %d\n",
				ret);
			goto irq_err;
		}

		ret = twl6040_request_irq(twl6040, TWL6040_IRQ_TH,
				twl6040_naudint_handler, 0,
				"twl6040_irq_thermal", twl6040);
		if (ret) {
			dev_err(twl6040->dev, "THERMAL IRQ request failed: %d\n",
				ret);
			goto thirq_err;
		}
	}

	/* dual-access registers controlled by I2C only */
	accctl = twl6040_reg_read(twl6040, TWL6040_REG_ACCCTL);
	twl6040_reg_write(twl6040, TWL6040_REG_ACCCTL, accctl | TWL6040_I2CSEL);

	if (pdata->get_ext_clk32k) {
		ret = pdata->get_ext_clk32k();
		if (ret) {
			dev_err(twl6040->dev,
				"failed to get external 32kHz clock %d\n",
				ret);
			goto clk32k_err;
		}
	}

	if (pdata->audio) {
		cell = &twl6040->cells[children];
		cell->name = "twl6040-codec";
		cell->platform_data = pdata->audio;
		cell->pdata_size = sizeof(*pdata->audio);
		children++;
	}

	if (pdata->vibra) {
		cell = &twl6040->cells[children];
		cell->name = "twl6040-vibra";
		cell->platform_data = pdata->vibra;
		cell->pdata_size = sizeof(*pdata->vibra);
		children++;
	}

	if (children) {
		ret = mfd_add_devices(&pdev->dev, pdev->id, twl6040->cells,
				      children, NULL, 0);
		if (ret)
			goto mfd_err;
	} else {
		dev_err(&pdev->dev, "No platform data found for children\n");
		ret = -ENODEV;
		goto mfd_err;
	}

	return 0;

mfd_err:
	if (pdata->put_ext_clk32k)
		pdata->put_ext_clk32k();
clk32k_err:
	if (naudint)
		twl6040_free_irq(twl6040, TWL6040_IRQ_TH, twl6040);
thirq_err:
	if (naudint)
		twl6040_free_irq(twl6040, TWL6040_IRQ_READY, twl6040);
irq_err:
	if (naudint)
		twl6040_irq_exit(twl6040);
gpio2_err:
	if (gpio_is_valid(audpwron))
		gpio_free(audpwron);
rev_err:
	if (pdata->exit)
		pdata->exit();
init_err:
	platform_set_drvdata(pdev, NULL);
	kfree(twl6040);
	return ret;
}

static int __devexit twl6040_remove(struct platform_device *pdev)
{
	struct twl6040 *twl6040 = platform_get_drvdata(pdev);
	struct twl4030_codec_data *pdata = dev_get_platdata(twl6040->dev);
	int audpwron = twl6040->audpwron;
	int naudint = twl6040->irq;

	twl6040_disable(twl6040);

	if (naudint) {
		twl6040_free_irq(twl6040, TWL6040_IRQ_READY, twl6040);
		twl6040_free_irq(twl6040, TWL6040_IRQ_TH, twl6040);
	}

	if (gpio_is_valid(audpwron))
		gpio_free(audpwron);

	if (naudint)
		twl6040_irq_exit(twl6040);

	mfd_remove_devices(&pdev->dev);

	if (pdata->put_ext_clk32k)
		pdata->put_ext_clk32k();

	if (pdata->exit)
		pdata->exit();

	platform_set_drvdata(pdev, NULL);
	kfree(twl6040);

	return 0;
}

static struct platform_driver twl6040_driver = {
	.probe		= twl6040_probe,
	.remove		= __devexit_p(twl6040_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "twl6040-audio",
	},
};

static int __devinit twl6040_init(void)
{
	return platform_driver_register(&twl6040_driver);
}
module_init(twl6040_init);

static void __devexit twl6040_exit(void)
{
	platform_driver_unregister(&twl6040_driver);
}

module_exit(twl6040_exit);

MODULE_DESCRIPTION("TWL6040 MFD");
MODULE_AUTHOR("Jorge Eduardo Candelaria <jorge.candelaria@ti.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:twl6040-audio");
