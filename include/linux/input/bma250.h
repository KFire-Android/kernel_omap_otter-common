#ifndef __BMA250_H__
#define __BMA250_H__

#define BMA250_DRIVER 		"bma250"

/* range and bandwidth */

#define BMA250_RANGE_2G         0x00
#define BMA250_RANGE_4G         0x05
#define BMA250_RANGE_8G         0x08
#define BMA250_RANGE_16G        0x0C

#define BMA250_SHIFT_RANGE_2G	6
#define BMA250_SHIFT_RANGE_4G	5
#define BMA250_SHIFT_RANGE_8G	4
#define BMA250_SHIFT_RANGE_16G	3

#define BMA250_BW_7_81HZ        0x08
#define BMA250_BW_15_63HZ       0x09
#define BMA250_BW_31_25HZ       0x0A
#define BMA250_BW_62_50HZ       0x0B
#define BMA250_BW_125HZ         0x0C
#define BMA250_BW_250HZ         0x0D
#define BMA250_BW_500HZ         0x0E
#define BMA250_BW_1000HZ        0x0F

/* mode settings */

#define BMA250_MODE_NORMAL      0
#define BMA250_MODE_LOWPOWER    1
#define BMA250_MODE_SUSPEND     2


struct bma250_platform_data {
	char 		*regulator_name;
	unsigned int	max_voltage;
	unsigned int	min_voltage;

	u8		range;
	u8		bandwidth;
	u8 		shift_adj;
	unsigned int	report_threshold; /* controls reporting to upper layer */
	unsigned int	poll_interval;
	unsigned int	max_interval;
	unsigned int	min_interval;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
};

#endif
