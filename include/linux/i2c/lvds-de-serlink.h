#ifndef __LINUX_LVDS_DSERLINK_H
#define __LINUX_LVDS_DSERLINK_H

/**
 * struct dserlink_platform_data - data to set up serlink driver
 */
struct dserlink_platform_data {
	unsigned mode_sel;
	unsigned interrupt;
	unsigned addr;
	unsigned data;
};

enum DSER_CMD {
	DSER_RESET,
	DSER_EN_BC,
	DSER_CONFIGURE,
	DSER_GET_I2C_ADDR,
	DSER_GET_SER_I2C_ADDR,
	DSER_READ,
	DSER_WRITE,
	DSER_REG_DUMP,
};

struct dser_i2c_data {
	u8 addr;
	u8 data;
};
#endif /* __LINUX_LVDS_DSERLINK_H */
