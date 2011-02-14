#ifndef __I2C_OMAP_H__
#define __I2C_OMAP_H__

#include <linux/platform_device.h>
#include <linux/pm_qos_params.h>

#define I2C_HAS_FASTMODE_PLUS	(1 << 0)

struct omap_i2c_bus_platform_data {
	u32	clkrate;
	struct	hwspinlock *handle;
	int	(*set_mpu_wkup_lat)(struct pm_qos_request_list **qos_request, long set);
	int	(*device_enable) (struct platform_device *pdev);
	int	(*device_shutdown) (struct platform_device *pdev);
	int	(*device_idle) (struct platform_device *pdev);
	int	(*hwspinlock_lock) (struct hwspinlock *handle);
	int	(*hwspinlock_unlock) (struct hwspinlock *handle);
	unsigned features;
};

#endif
