/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
//#define DEBUG

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/lp855x.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

/* LP8550/1/2/3/6 Registers */
#define LP855X_BRIGHTNESS_CTRL		0x00
#define LP855X_DEVICE_CTRL		0x01
#define LP855X_EEPROM_START		0xA0
#define LP855X_EEPROM_END		0xA7
#define LP8556_EPROM_START		0xA0
#define LP8556_EPROM_END		0xAF

/* LP8557 Registers */
#define LP8557_BL_CMD			0x00
#define LP8557_BL_MASK			0x01
#define LP8557_BL_ON			0x01
#define LP8557_BL_OFF			0x00
#define LP8557_BRIGHTNESS_CTRL		0x04
#define LP8557_CONFIG			0x10
#define LP8557_EPROM_START		0x10
#define LP8557_EPROM_END		0x1E

#define BUF_SIZE		20
#define DEFAULT_BL_NAME		"lcd-backlight"
#define MAX_BRIGHTNESS		255

#define BL_DRV_SUSPENDED		BL_CORE_DRIVER1

struct lp855x;

struct lp855x_device_config {
	u8 reg_brightness;
	u8 reg_devicectrl;
	int (*pre_init_device)(struct lp855x *);
	int (*post_init_device)(struct lp855x *);
};

struct lp855x {
	const char *chipname;
	enum lp855x_chip_id chip_id;
	struct lp855x_device_config *cfg;
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	struct lp855x_platform_data *pdata;
	struct regulator *regulator;
	bool gpio_enable;
};

static int lp855x_write_byte(struct lp855x *lp, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(lp->client, reg, data);
}

static int lp855x_read_byte(struct lp855x *lp, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	*data = (u8)ret;

	return 0;
}

static int lp855x_update_bit(struct lp855x *lp, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return lp855x_write_byte(lp, reg, tmp);
}

static bool lp855x_is_valid_rom_area(struct lp855x *lp, u8 addr)
{
	u8 start, end;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
		dev_dbg(lp->dev, "Define LP855x rom area\n");
		start = LP855X_EEPROM_START;
		end = LP855X_EEPROM_END;
		break;
	case LP8556:
		dev_dbg(lp->dev,"Define LP8556 rom area\n");
		start = LP8556_EPROM_START;
		end = LP8556_EPROM_END;
		break;
	case LP8557:
		dev_dbg(lp->dev, "Define LP8557 rom area\n");
		start = LP8557_EPROM_START;
		end = LP8557_EPROM_END;
		break;
	default:
		return false;
	}

	return (addr >= start && addr <= end);
}

static bool lp855x_check_init_device(struct lp855x *lp)
{
	struct lp855x_platform_data *pd = lp->pdata;
	int i, ret = 0;
	u8 addr, val;

	if (lp->gpio_enable && !gpio_get_value(pd->gpio_en))
		return false;

	if (lp->regulator && !regulator_is_enabled(lp->regulator))
		return false;

	/*Check device control reg first*/
	ret = lp855x_read_byte(lp, lp->cfg->reg_devicectrl, &val);
	if (ret)
		return false;

	if (pd->load_new_rom_data && pd->size_program) {
		for (i = 0; i < pd->size_program; i++) {
			addr = pd->rom_data[i].addr;

			if (!lp855x_is_valid_rom_area(lp, addr))
				continue;

			ret = lp855x_read_byte(lp, addr, &val);
			if (ret)
				return false;

			dev_dbg(lp->dev, "Check rom data 0x%02X 0x%02X\n",
				addr, val);

			if (val != pd->rom_data[i].val)
				return true;
		}
	}
	return 0;
}

static int lp855x_init_device(struct lp855x *lp)
{
	struct lp855x_platform_data *pd = lp->pdata;
	int i, ret, skip_preinit = 1;
	u8 addr, val;

	dev_dbg(lp->dev, "Power on LP855X\n");

	if (lp->gpio_enable && !gpio_get_value(pd->gpio_en))
		skip_preinit = 0;

	if (lp->regulator && !regulator_is_enabled(lp->regulator))
		skip_preinit = 0;

	if (skip_preinit)
		goto post_init;

	if (lp->gpio_enable)
		gpio_direction_output(pd->gpio_en, 1);

	if (lp->regulator)
		regulator_enable(lp->regulator);

	msleep(20);

	ret = (lp->cfg->pre_init_device) ? lp->cfg->pre_init_device(lp) : 0;
	if (ret) {
		dev_err(lp->dev, "pre init device err: %d\n", ret);
		return ret;
	}

	val = pd->device_control;
	dev_dbg(lp->dev, "Set %s configuration 0x%02x: 0x%02x\n",
		(lp->cfg->reg_devicectrl == LP8557_CONFIG) ?
			"LP8557" : "LP8550TO6",
		lp->cfg->reg_devicectrl, val);
	ret = lp855x_write_byte(lp, lp->cfg->reg_devicectrl, val);
	if (ret)
		return ret;

	if (pd->load_new_rom_data && pd->size_program) {
		for (i = 0; i < pd->size_program; i++) {
			addr = pd->rom_data[i].addr;
			val = pd->rom_data[i].val;
			if (!lp855x_is_valid_rom_area(lp, addr))
				continue;
			dev_dbg(lp->dev, "Load new rom data 0x%02X 0x%02X\n",
				addr, val);
			ret = lp855x_write_byte(lp, addr, val);
			if (ret)
				return ret;
		}
	}

post_init:
	ret = (lp->cfg->post_init_device) ? lp->cfg->post_init_device(lp) : 0;
	if (ret) {
		dev_err(lp->dev, "post init device err: %d\n", ret);
		return ret;
	}

	return 0;
}

static int lp855x_power_off(struct lp855x *lp)
{
	struct lp855x_platform_data *pd = lp->pdata;

	if (lp->gpio_enable)
		gpio_set_value(pd->gpio_en, 0);

	if (lp->regulator)
		regulator_disable(lp->regulator);

	return 0;
}

static int lp8557_bl_off(struct lp855x *lp)
{
	dev_dbg(lp->dev, "BL_ON = 0 before updating EPROM settings\n");
	/* BL_ON = 0 before updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_OFF);
}

static int lp8557_bl_on(struct lp855x *lp)
{
	dev_dbg(lp->dev, "BL_ON = 1 after updating EPROM settings\n");
	/* BL_ON = 1 after updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_ON);
}

static struct lp855x_device_config lp8550to6_cfg = {
	.reg_brightness = LP855X_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP855X_DEVICE_CTRL,
};

static struct lp855x_device_config lp8557_cfg = {
	.reg_brightness = LP8557_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP8557_CONFIG,
	.pre_init_device = lp8557_bl_off,
	.post_init_device = lp8557_bl_on,
};

static int lp855x_configure(struct lp855x *lp)
{
	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
	case LP8556:
	   dev_dbg(lp->dev, "Load lp8550to6 configuration\n");
		lp->cfg = &lp8550to6_cfg;
		break;
	case LP8557:
	   dev_dbg(lp->dev, "Load lp8557 configuration\n");
		lp->cfg = &lp8557_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int lp855x_bl_get_brightness(struct backlight_device *bl)
{
	struct backlight_properties *props = &bl->props;
	int max = min(props->max_brightness, props->max_thermal_brightness);

	return min(props->brightness, max);
}

static void lp855x_set_brightness(struct lp855x *lp)
{
	struct lp855x_pwm_data *pwm_data = &lp->pdata->pwm_data;
	struct backlight_properties *props = &lp->bl->props;
	int brightness, max;

	max = min(props->max_brightness, props->max_thermal_brightness);
	brightness = lp855x_bl_get_brightness(lp->bl);

	dev_dbg(lp->dev, "brightness=%d max=%d\n", brightness, max);

	switch (lp->pdata->mode) {
	case PWM_BASED:
		if (pwm_data->pwm_set_intensity)
			pwm_data->pwm_set_intensity(brightness, max);
		break;

	case REGISTER_BASED:
		lp855x_write_byte(lp, lp->cfg->reg_brightness, brightness);
		break;
	}
}

static int lp855x_bl_update_status(struct backlight_device *bl)
{
	struct lp855x *lp = bl_get_data(bl);
	struct backlight_properties *props = &bl->props;
	int power_new, power_old;

	power_new = !(props->state & BL_CORE_SUSPENDED);
	power_old = !(props->state & BL_DRV_SUSPENDED);

	/* Check for Power On */
	if (!power_old && power_new) {
		props->state &= ~BL_DRV_SUSPENDED;
		lp855x_init_device(lp);
	}

	/* Apply new brightness value only if device is not suspended */
	if (power_new || power_old)
		lp855x_set_brightness(lp);

	/* Check for Power Off */
	if (power_old && !power_new) {
		props->state |= BL_DRV_SUSPENDED;
		lp855x_power_off(lp);
	}

	return 0;
}

static const struct backlight_ops lp855x_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lp855x_bl_update_status,
	.get_brightness = lp855x_bl_get_brightness,
};

static int lp855x_backlight_register(struct lp855x *lp)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct lp855x_platform_data *pdata = lp->pdata;
	char *name = pdata->name ? : DEFAULT_BL_NAME;

	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS;
	props.state = 0;

	if (pdata->initial_brightness > props.max_brightness)
		pdata->initial_brightness = props.max_brightness;

	props.brightness = pdata->initial_brightness;

	bl = backlight_device_register(name, lp->dev, lp,
				       &lp855x_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	lp->bl = bl;

	return 0;
}

static void lp855x_backlight_unregister(struct lp855x *lp)
{
	if (lp->bl)
		backlight_device_unregister(lp->bl);
}

static ssize_t lp855x_get_chip_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	return scnprintf(buf, BUF_SIZE, "%s\n", lp->chipname);
}

static ssize_t lp855x_get_bl_ctl_mode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	enum lp855x_brightness_ctrl_mode mode = lp->pdata->mode;
	char *strmode = NULL;

	if (mode == PWM_BASED)
		strmode = "pwm based";
	else if (mode == REGISTER_BASED)
		strmode = "register based";

	return scnprintf(buf, BUF_SIZE, "%s\n", strmode);
}

static DEVICE_ATTR(chip_id, S_IRUGO, lp855x_get_chip_id, NULL);
static DEVICE_ATTR(bl_ctl_mode, S_IRUGO, lp855x_get_bl_ctl_mode, NULL);

static struct attribute *lp855x_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_bl_ctl_mode.attr,
	NULL,
};

static const struct attribute_group lp855x_attr_group = {
	.attrs = lp855x_attributes,
};

static int lp855x_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lp855x *lp;
	struct lp855x_platform_data *pdata = cl->dev.platform_data;
	enum lp855x_brightness_ctrl_mode mode;
	int ret;
	u8 val;

	if (!pdata) {
		dev_err(&cl->dev, "no platform data supplied\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp855x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	mode = pdata->mode;
	lp->client = cl;
	lp->dev = &cl->dev;
	lp->pdata = pdata;
	lp->chipname = id->name;
	lp->chip_id = id->driver_data;
	i2c_set_clientdata(cl, lp);

	if (pdata->gpio_en >= 0) {
		ret = gpio_request(pdata->gpio_en, "backlight_lp855x_gpio");
		if (ret != 0) {
			pr_err("backlight gpio request failed\n");
			goto err_dev;
		}
		lp->gpio_enable = true;
	}

	if (pdata->regulator_name) {
		lp->regulator = regulator_get(lp->dev, pdata->regulator_name);
		if (!lp->regulator) {
			dev_err(lp->dev, "Can't get regulator %s\n",
				pdata->regulator_name);
			goto err_dev;
		}
	}

	ret = lp855x_configure(lp);
	if (ret) {
		dev_err(lp->dev, "device config err: %d\n", ret);
		goto err_dev;
	}

	/* Force pro-init if needed */
	if (lp855x_check_init_device(lp))
		lp855x_power_off(lp);

	lp855x_init_device(lp);

	val = lp->pdata->initial_brightness;
	dev_dbg(lp->dev, "Initial %s brightness\n",
	        (lp->cfg->reg_brightness == LP855X_BRIGHTNESS_CTRL) ? "LP8550TO6" : "LP8557");
	ret = lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
	if (ret)
		goto err_dev;

	ret = lp855x_backlight_register(lp);
	if (ret) {
		dev_err(lp->dev,
			"failed to register backlight. err: %d\n", ret);
		goto err_dev;
	}

	ret = sysfs_create_group(&lp->dev->kobj, &lp855x_attr_group);
	if (ret) {
		dev_err(lp->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_sysfs;
	}

	backlight_update_status(lp->bl);

	return 0;

err_sysfs:
	lp855x_backlight_unregister(lp);
err_dev:
	if (lp->gpio_enable)
		gpio_free(lp->pdata->gpio_en);
	return ret;
}

static int lp855x_remove(struct i2c_client *client)
{
	struct lp855x *lp = i2c_get_clientdata(client);

	lp->bl->props.state |= BL_CORE_SUSPENDED;
	lp->bl->props.brightness = 0;
	backlight_update_status(lp->bl);

	if (lp->gpio_enable)
		gpio_free(lp->pdata->gpio_en);

	if (lp->regulator)
		regulator_put(lp->regulator);

	sysfs_remove_group(&lp->dev->kobj, &lp855x_attr_group);
	lp855x_backlight_unregister(lp);

	return 0;
}

static void lp855x_shutdown(struct i2c_client *client)
{
	struct lp855x *lp = i2c_get_clientdata(client);

	lp->bl->props.state |= BL_CORE_SUSPENDED;
	lp->bl->props.brightness = 0;
	backlight_update_status(lp->bl);
}

static const struct i2c_device_id lp855x_ids[] = {
	{"lp8550", LP8550},
	{"lp8551", LP8551},
	{"lp8552", LP8552},
	{"lp8553", LP8553},
	{"lp8556", LP8556},
	{"lp8557", LP8557},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static struct i2c_driver lp855x_driver = {
	.driver = {
		   .name = "lp855x",
		   },
	.probe = lp855x_probe,
	.remove = lp855x_remove,
	.id_table = lp855x_ids,
	.shutdown = lp855x_shutdown,
};

module_i2c_driver(lp855x_driver);

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
