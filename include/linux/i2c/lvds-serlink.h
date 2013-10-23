#ifndef __LINUX_LVDS_SERLINK_H
#define __LINUX_LVDS_SERLINK_H

/**
 * struct serlink_platform_data - data to set up serlink driver
 */
struct serlink_platform_data {
	unsigned mode_sel;
	unsigned interrupt;
	unsigned addr;
	unsigned data;
};

enum SER_CMD {
	SER_RESET,
	SER_CONFIGURE,
	SER_GET_I2C_ADDR,
	SER_VALIDATE_LINK,
	SER_VALIDATE_PCLK,
	SER_GET_DES_I2C_ADDR,
	SER_READ,
	SER_WRITE,
	SER_REG_DUMP,
};

struct ser_i2c_data {
	u8 addr;
	u8 data;
};
#endif /* __LINUX_LVDS_SERLINK_H */
