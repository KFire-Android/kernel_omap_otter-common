#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/i2c/twl.h>

#define OMAP4_PRM_RSTST			0x4A307B04
#define PHOENIX_START_CONDITION		0x1F
#define PHOENIX_STS_HW_CONDITIONS	0x21
#define PHOENIX_LAST_TURNOFF_STS	0x22

struct omap_boot_reason_priv {
	u8 pmic_start_condition;
	u8 pmic_last_turnoff_status;
	u8 pmic_status_hw_condition;
	u32 omap4_reset_reason;
};

static struct omap_boot_reason_priv dev_priv;

static ssize_t pmic_start_condition_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", dev_priv.pmic_start_condition);
}
static DEVICE_ATTR(pmic_start_condition, S_IRUGO, pmic_start_condition_show, NULL);

static ssize_t pmic_last_turnoff_status_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", dev_priv.pmic_last_turnoff_status);
}
static DEVICE_ATTR(pmic_last_turnoff_status, S_IRUGO, pmic_last_turnoff_status_show, NULL);

static ssize_t pmic_status_hw_condition_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", dev_priv.pmic_status_hw_condition);
}
static DEVICE_ATTR(pmic_status_hw_condition, S_IRUGO, pmic_status_hw_condition_show, NULL);

static ssize_t omap4_reset_reason_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", dev_priv.omap4_reset_reason);
}
static DEVICE_ATTR(omap4_reset_reason, S_IRUGO, omap4_reset_reason_show, NULL);

static int omap_boot_reason_probe(struct platform_device *pdev)
{
	int ret = -1;

	/* Read OMAP4 reset reason */
	dev_priv.omap4_reset_reason = omap_readl(OMAP4_PRM_RSTST);
	printk(KERN_INFO "OMAP4 reset reason is 0x%08x\n",
			dev_priv.omap4_reset_reason);

	/* Read PMIC start condition */
	twl_i2c_read_u8(TWL6030_MODULE_ID0,
		&dev_priv.pmic_start_condition, PHOENIX_START_CONDITION);
	printk(KERN_INFO
		"TWL6030: Start condition(PHOENIX_START_CONDITION) is 0x%02x\n",
		dev_priv.pmic_start_condition);

	/* Read PMIC last turnoff status */
	twl_i2c_read_u8(TWL6030_MODULE_ID0,
		&dev_priv.pmic_last_turnoff_status, PHOENIX_LAST_TURNOFF_STS);
	printk(KERN_INFO
		"TWL6030: Last turn off status (PHOENIX_LAST_TURN_OFF_STATUS) is 0x%02x\n",
		dev_priv.pmic_last_turnoff_status);

	/* Turn off the LPK bit, since we have logged it */
	twl_i2c_write_u8(TWL6030_MODULE_ID0,
		dev_priv.pmic_last_turnoff_status & ~(1 << 1), PHOENIX_LAST_TURNOFF_STS);

	/* Read PMIC HW conditions status */
	twl_i2c_read_u8(TWL6030_MODULE_ID0,
		&dev_priv.pmic_status_hw_condition, PHOENIX_STS_HW_CONDITIONS);
	printk(KERN_INFO
		"TWL6030: Hardware Conditions (PHOENIX_STS_HW_CONDITIONS) is 0x%02x\n",
		dev_priv.pmic_status_hw_condition);

	/* Create the sysfs files */
	ret = device_create_file(&pdev->dev, &dev_attr_pmic_start_condition);

	if (ret) {
		printk(KERN_ERR
			"Failed to create pmic_start_condition sysfs node\n");
		goto err4;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_pmic_last_turnoff_status);

	if (ret) {
		printk(KERN_ERR
			"Failed to create pmic_last_turnoff_status sysfs node\n");
		goto err3;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_pmic_status_hw_condition);

	if (ret) {
		printk(KERN_ERR
			"Failed to create pmic_status_hw_condition sysfs node\n");
		goto err2;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_omap4_reset_reason);

	if (ret) {
		printk(KERN_ERR
			"Failed to create omap4_reset_reason sysfs node\n");
		goto err1;
	}

	return 0;

err1:
	device_remove_file(&pdev->dev, &dev_attr_pmic_status_hw_condition);
err2:
	device_remove_file(&pdev->dev, &dev_attr_pmic_last_turnoff_status);
err3:
	device_remove_file(&pdev->dev, &dev_attr_pmic_start_condition);
err4:
	return ret;
}

static int omap_boot_reason_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_omap4_reset_reason);
	device_remove_file(&pdev->dev, &dev_attr_pmic_status_hw_condition);
	device_remove_file(&pdev->dev, &dev_attr_pmic_last_turnoff_status);
	device_remove_file(&pdev->dev, &dev_attr_pmic_start_condition);
	return 0;
}

static struct platform_device omap_boot_reason_device = {
	.name = "omap_boot_reason",
	.id = -1,
};

static struct platform_driver omap_boot_reason_driver = {
	.probe = omap_boot_reason_probe,
	.remove = omap_boot_reason_remove,
	.driver = {
		.name = "omap_boot_reason",
		.owner = THIS_MODULE,
	},
};

static int __init omap_boot_reason_init(void)
{
	int ret = -1;

	if ((ret = platform_driver_register(&omap_boot_reason_driver))) {
		printk(KERN_ERR"omap_boot_reason: "
			"Failed to register driver: %d\n", ret);
		return ret;
	}

	if ((ret = platform_device_register(&omap_boot_reason_device))) {
		printk(KERN_ERR "omap_boot_reason: "
			"Failed to register device: %d\n", ret);
		platform_driver_unregister(&omap_boot_reason_driver);
		return ret;
	}

	return 0;
}

static void __exit omap_boot_reason_exit(void)
{
	platform_device_unregister(&omap_boot_reason_device);
	platform_driver_unregister(&omap_boot_reason_driver);
}

module_init(omap_boot_reason_init);
module_exit(omap_boot_reason_exit);
