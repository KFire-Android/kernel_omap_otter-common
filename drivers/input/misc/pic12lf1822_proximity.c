/*
 * pic12lf1822_proximity -- Promixity Sensor for Microchip PIC12LF1822
 *
 * Copyright 2009-2011 Amazon Technologies Inc., All rights reserved
 * Manish Lachwani (lachwani@lab126.com)
 * Jerry Wong (wjerry@lab126.com)
 *
 */
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/sysdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <linux/input.h>

#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/irq.h>

#define PIC12LF1822_PROXIMITY_NAME      "PIC12LF1822_Prox"
#define PROXIMITY_THRESHOLD             10 /* 10 ms */
#define PIC12LF1822_PROXIMITY_I2C_ADDR  0x0D
#define PIC12LF1822_RETRY_DELAY         10
#define PIC12LF1822_MAX_RETRIES         5
#define GPIO_PIC12LF1822_PROXIMITY_IRQ  85
#define PROXIMITY_PORT_MINOR            161 /* /dev/proximity sensor */

/* Macro used to check bits for status register */
#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

/* Register Set for the PIC12LF1822 */
#define PIC12LF1822_FWVER         0x40
#define PIC12LF1822_RAWL          0x41
#define PIC12LF1822_RAWH          0x42
#define PIC12LF1822_AVGL          0x43
#define PIC12LF1822_AVGH          0x44
#define PIC12LF1822_PROXSTATE     0x45
#define PIC12LF1822_THRES         0x46
#define PIC12LF1822_DBNCDET       0x47
#define PIC12LF1822_WDTCON        0x48
#define PIC12LF1822_LOOPREAD      0x49
#define PIC12LF1822_ZOFFSET       0x4A
#define PIC12LF1822_UPSTEP        0x4B
#define PIC12LF1822_STATUS        0x4C
#define PIC12LF1822_I2CTIMEOUT    0x4D
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
#define PROXIMITY_MAX_REGISTER    0x4D
#else
#define PIC12LF1822_REFL          0x4E
#define PIC12LF1822_REFH          0x4F
#define PIC12LF1822_DEVIATEL      0x50
#define PIC12LF1822_DEVIATEH      0x51
#define PIC12LF1822_AVGINTERVAL   0x52
#define PROXIMITY_MAX_REGISTER    0x52
#endif

#undef DEBUGD
//#define DEBUGD
#ifdef DEBUGD
	#define dprintk(x...) printk(x)
#else
	#define dprintk(x...)
#endif

int gpio_proximity_detected(void);

static struct miscdevice proximity_misc_device = {
	PROXIMITY_PORT_MINOR,
	"proximitysensor",
	NULL,
};

struct pic12lf1822_proximity_info {
	struct i2c_client *client;
	struct input_dev *idev;
};

static struct input_dev *proximity_input = NULL;

static struct i2c_client *pic12lf1822_proximity_i2c_client;

static int pic12lf1822_proximity_read_i2c(u8 *id, u8 reg_num)
{
	s32 error;

	error = i2c_smbus_read_byte_data(pic12lf1822_proximity_i2c_client, reg_num);

	dprintk("pic12lf1822_proximity_read_i2c %d 0x%02x\n", reg_num, error);

	if (error < 0) {
		return -EIO;
	}

	*id = (error & 0xFF);
	return 0;
}

static int pic12lf1822_proximity_write_i2c(u8 reg_num, u8 data)
{
	dprintk("pic12lf1822_proximity_write_i2c %d 0x%02x\n", reg_num, data);

	return i2c_smbus_write_byte_data(pic12lf1822_proximity_i2c_client,
			reg_num, data);
}

static u8 proximity_reg_number = 0;

static void dump_regs(void)
{
	u8 i = 0;
	u8 value = 0;

	for (i = PIC12LF1822_FWVER; i <= PROXIMITY_MAX_REGISTER; i++) {
		pic12lf1822_proximity_read_i2c(&value, i);
		printk("Register 0x%x = Value:0x%x\n", i, value);
	}
}

static ssize_t proximity_reg_store(struct sys_device *dev,
		struct sysdev_attribute *attr,
				const char *buf, size_t size)
{
	u16 value = 0;

	if (sscanf(buf, "%hx", &value) <= 0) {
		printk(KERN_ERR "Could not store the codec register value\n");
		return -EINVAL;
	}

	proximity_reg_number = value;
	return size;
}

static ssize_t proximity_reg_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "Proximity Register Number: %d\n",
		proximity_reg_number);
	curr += sprintf(curr, "\n");

	return curr - buf;
}

static SYSDEV_ATTR(proximity_reg, 0666, proximity_reg_show, proximity_reg_store);

static struct sysdev_class proximity_reg_sysclass = {
	.name = "proximity_reg",
};

static struct sys_device device_proximity_reg = {
	.id = 0,
	.cls = &proximity_reg_sysclass,
};

static ssize_t proximity_register_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;
	u8 value = 0;

	if (proximity_reg_number > PROXIMITY_MAX_REGISTER) {
		curr += sprintf(curr, "Proximity Registers\n");
		curr += sprintf(curr, "\n");
		dump_regs();
	}
	else {
		pic12lf1822_proximity_read_i2c(&value, proximity_reg_number);
		curr += sprintf(curr, "Proximity Register %d\n",
				proximity_reg_number);
		curr += sprintf(curr, "\n");
		curr += sprintf(curr, "Value: 0x%x\n", value);
		curr += sprintf(curr, "\n");
	}

	return curr - buf;
}

static ssize_t proximity_register_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (proximity_reg_number > PROXIMITY_MAX_REGISTER) {
		printk(KERN_ERR "No codec register %d\n", proximity_reg_number);
		return size;
	}

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(proximity_reg_number, value);
	return size;
}

static SYSDEV_ATTR(proximity_register, 0666, proximity_register_show,
	proximity_register_store);

static struct sysdev_class proximity_register_sysclass = {
	.name = "proximity_register",
};

static struct sys_device device_proximity_register = {
	.id = 0,
	.cls = &proximity_register_sysclass,
};

static ssize_t prox_fwver_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_FWVER);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(fwver, 0666, prox_fwver_show, NULL);

static ssize_t prox_raw_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value_LSB, value_MSB;
	u16 value;

	pic12lf1822_proximity_read_i2c(&value_LSB, PIC12LF1822_RAWL);
	pic12lf1822_proximity_read_i2c(&value_MSB, PIC12LF1822_RAWH);
	value = (u16)(value_MSB << 8 | value_LSB);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(raw, 0666, prox_raw_show, NULL);

static ssize_t prox_avg_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value_LSB, value_MSB;
	u16 value;

	pic12lf1822_proximity_read_i2c(&value_LSB, PIC12LF1822_AVGL);
	pic12lf1822_proximity_read_i2c(&value_MSB, PIC12LF1822_AVGH);
	value = (u16)(value_MSB << 8| value_LSB);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(avg, 0666, prox_avg_show, NULL);

static ssize_t prox_proxstate_show(struct sys_device *dev,
	struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_PROXSTATE);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(proxstate, 0666, prox_proxstate_show, NULL);

static ssize_t prox_thres_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_THRES);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_thres_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_THRES, value);
	return size;
}

static SYSDEV_ATTR(thres, 0666, prox_thres_show, prox_thres_store);

static ssize_t prox_dbncdet_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_DBNCDET);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_dbncdet_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_DBNCDET, value);
	return size;
}

static SYSDEV_ATTR(dbncdet, 0666, prox_dbncdet_show, prox_dbncdet_store);

static ssize_t prox_wdtcon_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_WDTCON);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_wdtcon_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_WDTCON, value);
	return size;
}

static SYSDEV_ATTR(wdtcon, 0666, prox_wdtcon_show, prox_wdtcon_store);

static ssize_t prox_loopread_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_LOOPREAD);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_loopread_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_LOOPREAD, value);
	return size;
}

static SYSDEV_ATTR(loopread, 0666, prox_loopread_show, prox_loopread_store);

static ssize_t prox_zoffset_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_ZOFFSET);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_zoffset_store(struct sys_device *dev, struct sysdev_attribute *attr,
		const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_ZOFFSET, value);
	return size;
}

static SYSDEV_ATTR(zoffset, 0666, prox_zoffset_show, prox_zoffset_store);

static ssize_t prox_upstep_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_UPSTEP);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_upstep_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_UPSTEP, value);

	return size;
}

static SYSDEV_ATTR(upstep, 0666, prox_upstep_show, prox_upstep_store);

static ssize_t prox_detectstatus_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;
	//value = gpio_proximity_detected();
	if(!gpio_proximity_detected())
		value = 1;
	else
		value = 0;
	//pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_PROXSTATE);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(detectstatus, 0666, prox_detectstatus_show, NULL);


static ssize_t prox_status_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_STATUS);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_status_store(struct sys_device *dev, struct sysdev_attribute *attr,
					const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_STATUS, value);
	/* if "write to flash" bit (7) is set, a delay of 8ms must occur */
	if (CHECK_BIT(value, 7)) {
		dprintk(KERN_INFO "writing to flash, sleep 8ms\n");
		msleep(8);
	}

	return size;
}

static SYSDEV_ATTR(status, 0666, prox_status_show, prox_status_store);

static ssize_t prox_i2ctimeout_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_I2CTIMEOUT);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_i2ctimeout_store(struct sys_device *dev,
		struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_I2CTIMEOUT, value);
	return size;
}

static SYSDEV_ATTR(i2ctimeout, 0666, prox_i2ctimeout_show, prox_i2ctimeout_store);

#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
static ssize_t prox_enable_show(struct sys_device *dev,
	struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_WDTCON);
	return sprintf(buf, "%d\n", value & 0x1);
}

static ssize_t prox_enable_store(struct sys_device *dev,
	struct sysdev_attribute *attr, const char *buf, size_t size)
{
	u8 value;
	u8 enable;

	// Read original setting then overwrite mode-bit
	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_WDTCON);

	if (sscanf(buf, "%hhx", &enable) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}
	if(enable) {
		value |= 1;
	}
	else {
		value &= 0xFE;
	}
	pic12lf1822_proximity_write_i2c(PIC12LF1822_WDTCON, value);
	return size;
}

static SYSDEV_ATTR(enable, 0644, prox_enable_show, prox_enable_store);

static ssize_t prox_ref_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value_LSB, value_MSB;
	u16 value;

	pic12lf1822_proximity_read_i2c(&value_LSB, PIC12LF1822_REFL);
	pic12lf1822_proximity_read_i2c(&value_MSB, PIC12LF1822_REFH);
	value = (u16)(value_MSB << 8 | value_LSB);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(ref, 0666, prox_ref_show, NULL);

static ssize_t prox_deviate_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value_LSB, value_MSB;
	u16 value;

	pic12lf1822_proximity_read_i2c(&value_LSB, PIC12LF1822_DEVIATEL);
	pic12lf1822_proximity_read_i2c(&value_MSB, PIC12LF1822_DEVIATEH);
	value = (u16)(value_MSB << 8 | value_LSB);
	return sprintf(buf, "%d\n", value);
}

static SYSDEV_ATTR(deviate, 0666, prox_deviate_show, NULL);


static ssize_t prox_avgint_show(struct sys_device *dev,
		struct sysdev_attribute *attr, char *buf)
{
	u8 value;

	pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_AVGINTERVAL);
	return sprintf(buf, "%d\n", value);
}

static ssize_t prox_avgint_store(struct sys_device *dev, struct sysdev_attribute *attr,
		const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	pic12lf1822_proximity_write_i2c(PIC12LF1822_AVGINTERVAL, value);
	return size;
}

static SYSDEV_ATTR(avgint, 0666, prox_avgint_show, prox_avgint_store);
#endif

static struct sysdev_class proximity_sysclass = {
	.name = "proximity",
};

static struct sys_device device_proximity = {
	.id = 0,
	.cls = &proximity_sysclass,
};

void gpio_proximity_reset(void)
{
	/* TODO no reset pin */
	dprintk(KERN_INFO "gpio_proximity_reset\n");

}

int gpio_proximity_int(void)
{

	dprintk(KERN_INFO "gpio_proximity_int\n");

//	return IOMUX_TO_IRQ(MX50_PIN_DISP_D0);
	return OMAP_GPIO_IRQ(GPIO_PIC12LF1822_PROXIMITY_IRQ);
//	return device->client->irq;
}

int gpio_proximity_detected(void)
{
	return gpio_get_value(GPIO_PIC12LF1822_PROXIMITY_IRQ);
//	return OMAP_GPIO_BIT(GPIO_PIC12LF1822_PROXIMITY_IRQ, 0);
//	return 0;
}

/**
 *      Forces the device to its lowest power settings.
 *
 *  In this mode, the device still makes lux measurements,
 *  but does not send any interrupts.
 *
 *      @param struct pic12lf1822_proximity_info *info
 *        points to a representation of a pic12lf1822_proximity device in memory
 *      @return
 *    0 if no error, error code otherwise
 *
 */
static int pic12lf1822_proximity_sleep(struct pic12lf1822_proximity_info *info)
{
    u8 value;

    pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_WDTCON);
    value &= 0xfe; // Enter low power mode (clear bit 0)
    pic12lf1822_proximity_write_i2c(PIC12LF1822_WDTCON, value);

    return 0;

} // initialization


/**
 *      Wakes the device from its lowest power settings to run.
 *
 *      @param struct pic12lf1822_proximity_info *info
 *        points to a represenation of a pic12lf1822_proximity device in memory
 *      @return
 *    0 if no error, error code otherwise
 *
 */
static int pic12lf1822_proximity_wake(struct pic12lf1822_proximity_info *info)
{
    u8 value;

    pic12lf1822_proximity_read_i2c(&value, PIC12LF1822_WDTCON);
    value |= 0x01; // exit low power mode (set bit 0)
    pic12lf1822_proximity_write_i2c(PIC12LF1822_WDTCON, value);

    return 0;

} // initialization

/**
 *  Shutdown function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *              the associated I2C client for the device
 *
 *      @return
 *    void
 */
static int pic12lf1822_proximity_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct pic12lf1822_proximity_info *info;
	u8 touch_status = 0;
	int error = 0;

//#define PIC12LF1822_DEMO
#ifdef PIC12LF1822_DEMO
	int i;
#endif // PIC12LF1822_DEMO

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		return -ENOMEM;
	}

	/*TODO Reset Proximity */
	gpio_proximity_reset();

	i2c_set_clientdata(client, info);
	info->client = client;
	pic12lf1822_proximity_i2c_client = info->client;
	pic12lf1822_proximity_i2c_client->addr = client->addr;
	proximity_input = input_allocate_device();
	if (!proximity_input)
	{
		dev_err(&client->dev, "failed to allocate input device\n");
		kfree(info);
		return -ENOMEM;
	}
	proximity_input->name = PIC12LF1822_PROXIMITY_NAME;
	info->idev = proximity_input;
	input_set_capability(proximity_input, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(proximity_input, ABS_DISTANCE, 0, 1, 0, 0);
	error = input_register_device(proximity_input);
	if (error)
	{
		dev_err(&client->dev, "failed to register input device\n");
		input_free_device(proximity_input);
		kfree(info);
		return error;
	}


#ifdef PIC12LF1822_DEMO
	for (i = 0; i < 1000; i++)
	{
		dump_regs();

		msleep(500);
	}
#endif // PIC12LF1822_DEMO

	if (pic12lf1822_proximity_read_i2c(&touch_status, PIC12LF1822_FWVER) < 0)
	{
		printk(KERN_ERR "Proximity Probe I2C Read Error\n");
		goto fail;
	}

	printk(KERN_INFO "Proximity Sensor Found, Firmware Ver.: 0x%x\n", touch_status);
	dump_regs();
	pic12lf1822_proximity_wake(info);
	/* /sys files */
	error = sysdev_class_register(&proximity_reg_sysclass);
	if (error) goto exit;
	error = sysdev_register(&device_proximity_reg);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity_reg, &attr_proximity_reg);
	if (error) goto exit;

	error = sysdev_class_register(&proximity_register_sysclass);
	if (error) goto exit;
	error = sysdev_register(&device_proximity_register);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity_register,
		&attr_proximity_register);
	if (error) goto exit;

	error = sysdev_class_register(&proximity_sysclass);
	if (error) goto exit;
	error = sysdev_register(&device_proximity);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_fwver);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_raw);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_avg);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_proxstate);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_detectstatus);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_thres);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_dbncdet);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_wdtcon);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_loopread);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_zoffset);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_upstep);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_status);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_i2ctimeout);
	if (error) goto exit;
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
	error = sysdev_create_file(&device_proximity, &attr_enable);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_ref);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_deviate);
	if (error) goto exit;
	error = sysdev_create_file(&device_proximity, &attr_avgint);
	if (error) goto exit;
#endif
	return 0;

exit:
	printk(KERN_INFO "Proximity Probe Sysdev Create File Error %d\n", error);
	return -error;
fail:
	dev_err(&client->dev, "No Proximity Sensor Found\n");
	input_unregister_device(proximity_input);
	input_free_device(proximity_input);
	kfree(info);
	return -ENODEV;
}

static int pic12lf1822_proximity_remove(struct i2c_client *client)
{
	struct pic12lf1822_proximity_info *info = i2c_get_clientdata(client);

	i2c_set_clientdata(client, info);

	sysdev_remove_file(&device_proximity_reg, &attr_proximity_reg);
	sysdev_remove_file(&device_proximity_register, &attr_proximity_register);
	sysdev_remove_file(&device_proximity, &attr_fwver);
	sysdev_remove_file(&device_proximity, &attr_raw);
	sysdev_remove_file(&device_proximity, &attr_avg);
	sysdev_remove_file(&device_proximity, &attr_proxstate);
	sysdev_remove_file(&device_proximity, &attr_detectstatus);
	sysdev_remove_file(&device_proximity, &attr_thres);
	sysdev_remove_file(&device_proximity, &attr_dbncdet);
	sysdev_remove_file(&device_proximity, &attr_wdtcon);
	sysdev_remove_file(&device_proximity, &attr_loopread);
	sysdev_remove_file(&device_proximity, &attr_zoffset);
	sysdev_remove_file(&device_proximity, &attr_upstep);
	sysdev_remove_file(&device_proximity, &attr_status);
	sysdev_remove_file(&device_proximity, &attr_i2ctimeout);
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
	sysdev_remove_file(&device_proximity, &attr_enable);
	sysdev_remove_file(&device_proximity, &attr_ref);
	sysdev_remove_file(&device_proximity, &attr_deviate);
	sysdev_remove_file(&device_proximity, &attr_avgint);
#endif
	return 0;
}

static void pic12lf1822_proximity_shutdown(struct i2c_client *client)
{
	printk(KERN_INFO "pic12lf1822_proximity_shutdown\n");

	pic12lf1822_proximity_remove(client);

	return;
} // pic12lf1822_proximity_shutdown


/**
 *  Suspend function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *              the associated I2C client for the device
 *
 *      @return
 *    0
 */
static int pic12lf1822_proximity_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct pic12lf1822_proximity_info *info = i2c_get_clientdata(client);

	printk(KERN_INFO "pic12lf1822_proximity_suspend\n");

	pic12lf1822_proximity_sleep(info);

	return 0;
} // pic12lf1822_proximity_suspend


/**
 *  Resume function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *              the associated I2C client for the device
 *
 *      @return
 *    0
 */
static int pic12lf1822_proximity_resume(struct i2c_client *client)
{
	struct pic12lf1822_proximity_info *info = i2c_get_clientdata(client);

	printk(KERN_INFO "pic12lf1822_proximity_resume\n");

	pic12lf1822_proximity_wake(info);

	return 0;
} // pic12lf1822_proximity_resume


static const struct i2c_device_id pic12lf1822_proximity_id[] =  {
	{ PIC12LF1822_PROXIMITY_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, pic12lf1822_proximity_id);

static struct i2c_driver proximity_i2c_driver = {
	.driver = {
			.name = PIC12LF1822_PROXIMITY_NAME,
		},
	.suspend = pic12lf1822_proximity_suspend,
	.resume = pic12lf1822_proximity_resume,
	.shutdown = pic12lf1822_proximity_shutdown,
	.probe = pic12lf1822_proximity_probe,
	.remove = pic12lf1822_proximity_remove,
	.id_table = pic12lf1822_proximity_id,
};

static void do_proximity_work(struct work_struct *dummy)
{
	int irq = gpio_proximity_int();

	dprintk(KERN_INFO "do_proximity_work\n");
	printk(KERN_INFO "Received Proximity Sensor Interrupt\n");

	if (!gpio_proximity_detected()) {
		kobject_uevent(&proximity_misc_device.this_device->kobj, KOBJ_ONLINE);
		input_report_abs(proximity_input, ABS_DISTANCE, 0);
		input_sync(proximity_input);
		irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
	}
	else {
		kobject_uevent(&proximity_misc_device.this_device->kobj, KOBJ_OFFLINE);
		input_report_abs(proximity_input, ABS_DISTANCE, 1);
		input_sync(proximity_input);
		irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);
	}

	enable_irq(irq);
}

static DECLARE_DELAYED_WORK(proximity_work, do_proximity_work);

static irqreturn_t proximity_irq(int irq, void *devid)
{
	dprintk(KERN_INFO "proximity_irq\n");

	disable_irq_nosync(irq);

	/* Debounce for 10 ms */
	schedule_delayed_work(&proximity_work, msecs_to_jiffies(PROXIMITY_THRESHOLD));

	return IRQ_HANDLED;
}

static int __init proximity_port_init(void)
{
	int err = 0;
	int irq = gpio_proximity_int();

	dprintk(KERN_INFO "*******************\n");
	dprintk(KERN_INFO "*******************\n");
	dprintk(KERN_INFO "*******************\n");
	dprintk(KERN_INFO "proximity_port_init\n");
	dprintk(KERN_INFO "*******************\n");
	dprintk(KERN_INFO "*******************\n");
	dprintk(KERN_INFO "*******************\n");
	printk(KERN_INFO "Initializing PIC12LF1822 Proximity Sensor\n");

	err = request_irq(irq, proximity_irq, IRQF_TRIGGER_FALLING,
					PIC12LF1822_PROXIMITY_NAME, NULL);

	if (err != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get IRQ %d %d\n", irq, err);
		return err;
	}

	if (misc_register(&proximity_misc_device)) {
		printk (KERN_ERR "proximity_misc_device: Couldn't register device 10, "
				"%d.\n", PROXIMITY_PORT_MINOR);
		return -EBUSY;
	}

	err = i2c_add_driver(&proximity_i2c_driver);

	if (err) {
		printk(KERN_ERR "Proximity: Could not add driver\n");
		free_irq(gpio_proximity_int(), NULL);
	}

	if (!gpio_proximity_detected())
		kobject_uevent(&proximity_misc_device.this_device->kobj, KOBJ_ONLINE);
	else
		kobject_uevent(&proximity_misc_device.this_device->kobj, KOBJ_OFFLINE);

	return err;
}

static void proximity_port_cleanup(void)
{
	misc_deregister(&proximity_misc_device);
	free_irq(gpio_proximity_int(), NULL);
	i2c_del_driver(&proximity_i2c_driver);
}

static void __exit proximity_port_remove(void)
{
	proximity_port_cleanup();
}

module_init(proximity_port_init);
module_exit(proximity_port_remove);

MODULE_AUTHOR("Manish Lachwani/Jerry Wong");
MODULE_DESCRIPTION("PIC12LF1822 Proximity Sensor");
MODULE_LICENSE("GPL");

