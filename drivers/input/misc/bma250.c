/*  Date: 2011/3/03 17:40:00
 *  Revision: 2.3
 */

/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file BMA250.c
   brief This file contains all function implementations for the BMA250 in linux

*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/twl.h>
#include <linux/gpio.h>
#include <linux/irq.h>


#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma250_early_suspend(struct early_suspend *h);
static void bma250_late_resume(struct early_suspend *h);
#endif

extern u8 quanta_get_mbid(void);	//leon add to check the board id

#define SENSOR_NAME 			"bma250"

#define ABSMIN				-512
#define ABSMAX				512
#define SLOPE_THRESHOLD_VALUE 		32
#define SLOPE_DURATION_VALUE 		1
#define INTERRUPT_LATCH_MODE 		13
#define INTERRUPT_ENABLE 		1
#define INTERRUPT_DISABLE 		0
#define MAP_SLOPE_INTERRUPT 		2
#define SLOPE_X_INDEX 			5
#define SLOPE_Y_INDEX 			6
#define SLOPE_Z_INDEX 			7
#define BMA250_MAX_DELAY		50
#define BMA250_CHIP_ID			3
#define BMA250_RANGE_SET		0
#define BMA250_BW_SET			1 /* this corresponds to 32ms. Since we poll at 50ms interval we don't need higher frequency data */
#define BMA250_REPORT_SET		5 /* value is from 5 to 10 */ 


/*
 *
 *      register definitions
 *
 */
#define LOW_G_INTERRUPT				REL_Z
#define HIGH_G_INTERRUPT 			REL_HWHEEL
#define SLOP_INTERRUPT 				REL_DIAL
#define DOUBLE_TAP_INTERRUPT 			REL_WHEEL
#define SINGLE_TAP_INTERRUPT 			REL_MISC
#define ORIENT_INTERRUPT 			ABS_PRESSURE
#define FLAT_INTERRUPT 				ABS_DISTANCE


#define HIGH_G_INTERRUPT_X_HAPPENED			1
#define HIGH_G_INTERRUPT_Y_HAPPENED 			2
#define HIGH_G_INTERRUPT_Z_HAPPENED 			3
#define HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED 		4
#define HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED		5
#define HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED 		6
#define SLOPE_INTERRUPT_X_HAPPENED 			7
#define SLOPE_INTERRUPT_Y_HAPPENED 			8
#define SLOPE_INTERRUPT_Z_HAPPENED 			9
#define SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED 		10
#define SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED 		11
#define SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED 		12
#define DOUBLE_TAP_INTERRUPT_HAPPENED 			13
#define SINGLE_TAP_INTERRUPT_HAPPENED 			14
#define UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED 		15
#define UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED	 	16
#define UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED 	17
#define UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED	18
#define DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED 	19
#define DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED 	20
#define DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED 	21
#define DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED 	22
#define FLAT_INTERRUPT_TURE_HAPPENED			23
#define FLAT_INTERRUPT_FALSE_HAPPENED			24
#define LOW_G_INTERRUPT_HAPPENED			25

#define PAD_LOWG					0
#define PAD_HIGHG					1
#define PAD_SLOP					2
#define PAD_DOUBLE_TAP					3
#define PAD_SINGLE_TAP					4
#define PAD_ORIENT					5
#define PAD_FLAT					6


#define BMA250_CHIP_ID_REG                      0x00
#define BMA250_VERSION_REG                      0x01
#define BMA250_X_AXIS_LSB_REG                   0x02
#define BMA250_X_AXIS_MSB_REG                   0x03
#define BMA250_Y_AXIS_LSB_REG                   0x04
#define BMA250_Y_AXIS_MSB_REG                   0x05
#define BMA250_Z_AXIS_LSB_REG                   0x06
#define BMA250_Z_AXIS_MSB_REG                   0x07
#define BMA250_TEMP_RD_REG                      0x08
#define BMA250_STATUS1_REG                      0x09
#define BMA250_STATUS2_REG                      0x0A
#define BMA250_STATUS_TAP_SLOPE_REG             0x0B
#define BMA250_STATUS_ORIENT_HIGH_REG           0x0C
#define BMA250_RANGE_SEL_REG                    0x0F
#define BMA250_BW_SEL_REG                       0x10
#define BMA250_MODE_CTRL_REG                    0x11
#define BMA250_LOW_NOISE_CTRL_REG               0x12
#define BMA250_DATA_CTRL_REG                    0x13
#define BMA250_RESET_REG                        0x14
#define BMA250_INT_ENABLE1_REG                  0x16
#define BMA250_INT_ENABLE2_REG                  0x17
#define BMA250_INT1_PAD_SEL_REG                 0x19
#define BMA250_INT_DATA_SEL_REG                 0x1A
#define BMA250_INT2_PAD_SEL_REG                 0x1B
#define BMA250_INT_SRC_REG                      0x1E
#define BMA250_INT_SET_REG                      0x20
#define BMA250_INT_CTRL_REG                     0x21
#define BMA250_LOW_DURN_REG                     0x22
#define BMA250_LOW_THRES_REG                    0x23
#define BMA250_LOW_HIGH_HYST_REG                0x24
#define BMA250_HIGH_DURN_REG                    0x25
#define BMA250_HIGH_THRES_REG                   0x26
#define BMA250_SLOPE_DURN_REG                   0x27
#define BMA250_SLOPE_THRES_REG                  0x28
#define BMA250_TAP_PARAM_REG                    0x2A
#define BMA250_TAP_THRES_REG                    0x2B
#define BMA250_ORIENT_PARAM_REG                 0x2C
#define BMA250_THETA_BLOCK_REG                  0x2D
#define BMA250_THETA_FLAT_REG                   0x2E
#define BMA250_FLAT_HOLD_TIME_REG               0x2F
#define BMA250_STATUS_LOW_POWER_REG             0x31
#define BMA250_SELF_TEST_REG                    0x32
#define BMA250_EEPROM_CTRL_REG                  0x33
#define BMA250_SERIAL_CTRL_REG                  0x34
#define BMA250_CTRL_UNLOCK_REG                  0x35
#define BMA250_OFFSET_CTRL_REG                  0x36
#define BMA250_OFFSET_PARAMS_REG                0x37
#define BMA250_OFFSET_FILT_X_REG                0x38
#define BMA250_OFFSET_FILT_Y_REG                0x39
#define BMA250_OFFSET_FILT_Z_REG                0x3A
#define BMA250_OFFSET_UNFILT_X_REG              0x3B
#define BMA250_OFFSET_UNFILT_Y_REG              0x3C
#define BMA250_OFFSET_UNFILT_Z_REG              0x3D
#define BMA250_SPARE_0_REG                      0x3E
#define BMA250_SPARE_1_REG                      0x3F


#define BMA250_ACC_X_LSB__POS              6
#define BMA250_ACC_X_LSB__LEN              2
#define BMA250_ACC_X_LSB__MSK              0xC0
#define BMA250_ACC_X_LSB__REG              BMA250_X_AXIS_LSB_REG

#define BMA250_ACC_X_MSB__POS              0
#define BMA250_ACC_X_MSB__LEN              8
#define BMA250_ACC_X_MSB__MSK              0xFF
#define BMA250_ACC_X_MSB__REG              BMA250_X_AXIS_MSB_REG

#define BMA250_ACC_Y_LSB__POS              6
#define BMA250_ACC_Y_LSB__LEN              2
#define BMA250_ACC_Y_LSB__MSK              0xC0
#define BMA250_ACC_Y_LSB__REG              BMA250_Y_AXIS_LSB_REG

#define BMA250_ACC_Y_MSB__POS              0
#define BMA250_ACC_Y_MSB__LEN              8
#define BMA250_ACC_Y_MSB__MSK              0xFF
#define BMA250_ACC_Y_MSB__REG              BMA250_Y_AXIS_MSB_REG

#define BMA250_ACC_Z_LSB__POS              6
#define BMA250_ACC_Z_LSB__LEN              2
#define BMA250_ACC_Z_LSB__MSK              0xC0
#define BMA250_ACC_Z_LSB__REG              BMA250_Z_AXIS_LSB_REG

#define BMA250_ACC_Z_MSB__POS              0
#define BMA250_ACC_Z_MSB__LEN              8
#define BMA250_ACC_Z_MSB__MSK              0xFF
#define BMA250_ACC_Z_MSB__REG              BMA250_Z_AXIS_MSB_REG

#define BMA250_RANGE_SEL__POS              0
#define BMA250_RANGE_SEL__LEN              4
#define BMA250_RANGE_SEL__MSK              0x0F
#define BMA250_RANGE_SEL__REG              BMA250_RANGE_SEL_REG

#define BMA250_BANDWIDTH__POS              0
#define BMA250_BANDWIDTH__LEN              5
#define BMA250_BANDWIDTH__MSK              0x1F
#define BMA250_BANDWIDTH__REG              BMA250_BW_SEL_REG

#define BMA250_EN_LOW_POWER__POS           6
#define BMA250_EN_LOW_POWER__LEN           1
#define BMA250_EN_LOW_POWER__MSK           0x40
#define BMA250_EN_LOW_POWER__REG           BMA250_MODE_CTRL_REG

#define BMA250_EN_SUSPEND__POS             7
#define BMA250_EN_SUSPEND__LEN             1
#define BMA250_EN_SUSPEND__MSK             0x80
#define BMA250_EN_SUSPEND__REG             BMA250_MODE_CTRL_REG

#define BMA250_INT_MODE_SEL__POS           0
#define BMA250_INT_MODE_SEL__LEN           4
#define BMA250_INT_MODE_SEL__MSK           0x0F
#define BMA250_INT_MODE_SEL__REG           BMA250_INT_CTRL_REG

#define BMA250_LOWG_INT_S__POS             0
#define BMA250_LOWG_INT_S__LEN             1
#define BMA250_LOWG_INT_S__MSK             0x01
#define BMA250_LOWG_INT_S__REG             BMA250_STATUS1_REG

#define BMA250_HIGHG_INT_S__POS            1
#define BMA250_HIGHG_INT_S__LEN            1
#define BMA250_HIGHG_INT_S__MSK            0x02
#define BMA250_HIGHG_INT_S__REG            BMA250_STATUS1_REG

#define BMA250_SLOPE_INT_S__POS            2
#define BMA250_SLOPE_INT_S__LEN            1
#define BMA250_SLOPE_INT_S__MSK            0x04
#define BMA250_SLOPE_INT_S__REG            BMA250_STATUS1_REG

#define BMA250_DOUBLE_TAP_INT_S__POS       4
#define BMA250_DOUBLE_TAP_INT_S__LEN       1
#define BMA250_DOUBLE_TAP_INT_S__MSK       0x10
#define BMA250_DOUBLE_TAP_INT_S__REG       BMA250_STATUS1_REG

#define BMA250_SINGLE_TAP_INT_S__POS       5
#define BMA250_SINGLE_TAP_INT_S__LEN       1
#define BMA250_SINGLE_TAP_INT_S__MSK       0x20
#define BMA250_SINGLE_TAP_INT_S__REG       BMA250_STATUS1_REG

#define BMA250_ORIENT_INT_S__POS           6
#define BMA250_ORIENT_INT_S__LEN           1
#define BMA250_ORIENT_INT_S__MSK           0x40
#define BMA250_ORIENT_INT_S__REG           BMA250_STATUS1_REG

#define BMA250_FLAT_INT_S__POS             7
#define BMA250_FLAT_INT_S__LEN             1
#define BMA250_FLAT_INT_S__MSK             0x80
#define BMA250_FLAT_INT_S__REG             BMA250_STATUS1_REG

#define BMA250_DATA_INT_S__POS             7
#define BMA250_DATA_INT_S__LEN             1
#define BMA250_DATA_INT_S__MSK             0x80
#define BMA250_DATA_INT_S__REG             BMA250_STATUS2_REG

#define BMA250_SLOPE_FIRST_X__POS          0
#define BMA250_SLOPE_FIRST_X__LEN          1
#define BMA250_SLOPE_FIRST_X__MSK          0x01
#define BMA250_SLOPE_FIRST_X__REG          BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_SLOPE_FIRST_Y__POS          1
#define BMA250_SLOPE_FIRST_Y__LEN          1
#define BMA250_SLOPE_FIRST_Y__MSK          0x02
#define BMA250_SLOPE_FIRST_Y__REG          BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_SLOPE_FIRST_Z__POS          2
#define BMA250_SLOPE_FIRST_Z__LEN          1
#define BMA250_SLOPE_FIRST_Z__MSK          0x04
#define BMA250_SLOPE_FIRST_Z__REG          BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_SLOPE_SIGN_S__POS           3
#define BMA250_SLOPE_SIGN_S__LEN           1
#define BMA250_SLOPE_SIGN_S__MSK           0x08
#define BMA250_SLOPE_SIGN_S__REG           BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_TAP_FIRST_X__POS            4
#define BMA250_TAP_FIRST_X__LEN            1
#define BMA250_TAP_FIRST_X__MSK            0x10
#define BMA250_TAP_FIRST_X__REG            BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_TAP_FIRST_Y__POS            5
#define BMA250_TAP_FIRST_Y__LEN            1
#define BMA250_TAP_FIRST_Y__MSK            0x20
#define BMA250_TAP_FIRST_Y__REG            BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_TAP_FIRST_Z__POS            6
#define BMA250_TAP_FIRST_Z__LEN            1
#define BMA250_TAP_FIRST_Z__MSK            0x40
#define BMA250_TAP_FIRST_Z__REG            BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_TAP_FIRST_XYZ__POS          4
#define BMA250_TAP_FIRST_XYZ__LEN          3
#define BMA250_TAP_FIRST_XYZ__MSK          0x70
#define BMA250_TAP_FIRST_XYZ__REG          BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_TAP_SIGN_S__POS             7
#define BMA250_TAP_SIGN_S__LEN             1
#define BMA250_TAP_SIGN_S__MSK             0x80
#define BMA250_TAP_SIGN_S__REG             BMA250_STATUS_TAP_SLOPE_REG

#define BMA250_HIGHG_FIRST_X__POS          0
#define BMA250_HIGHG_FIRST_X__LEN          1
#define BMA250_HIGHG_FIRST_X__MSK          0x01
#define BMA250_HIGHG_FIRST_X__REG          BMA250_STATUS_ORIENT_HIGH_REG

#define BMA250_HIGHG_FIRST_Y__POS          1
#define BMA250_HIGHG_FIRST_Y__LEN          1
#define BMA250_HIGHG_FIRST_Y__MSK          0x02
#define BMA250_HIGHG_FIRST_Y__REG          BMA250_STATUS_ORIENT_HIGH_REG

#define BMA250_HIGHG_FIRST_Z__POS          2
#define BMA250_HIGHG_FIRST_Z__LEN          1
#define BMA250_HIGHG_FIRST_Z__MSK          0x04
#define BMA250_HIGHG_FIRST_Z__REG          BMA250_STATUS_ORIENT_HIGH_REG

#define BMA250_HIGHG_SIGN_S__POS           3
#define BMA250_HIGHG_SIGN_S__LEN           1
#define BMA250_HIGHG_SIGN_S__MSK           0x08
#define BMA250_HIGHG_SIGN_S__REG           BMA250_STATUS_ORIENT_HIGH_REG

#define BMA250_ORIENT_S__POS               4
#define BMA250_ORIENT_S__LEN               3
#define BMA250_ORIENT_S__MSK               0x70
#define BMA250_ORIENT_S__REG               BMA250_STATUS_ORIENT_HIGH_REG

#define BMA250_FLAT_S__POS                 7
#define BMA250_FLAT_S__LEN                 1
#define BMA250_FLAT_S__MSK                 0x80
#define BMA250_FLAT_S__REG                 BMA250_STATUS_ORIENT_HIGH_REG

#define BMA250_EN_SLOPE_X_INT__POS         0
#define BMA250_EN_SLOPE_X_INT__LEN         1
#define BMA250_EN_SLOPE_X_INT__MSK         0x01
#define BMA250_EN_SLOPE_X_INT__REG         BMA250_INT_ENABLE1_REG

#define BMA250_EN_SLOPE_Y_INT__POS         1
#define BMA250_EN_SLOPE_Y_INT__LEN         1
#define BMA250_EN_SLOPE_Y_INT__MSK         0x02
#define BMA250_EN_SLOPE_Y_INT__REG         BMA250_INT_ENABLE1_REG

#define BMA250_EN_SLOPE_Z_INT__POS         2
#define BMA250_EN_SLOPE_Z_INT__LEN         1
#define BMA250_EN_SLOPE_Z_INT__MSK         0x04
#define BMA250_EN_SLOPE_Z_INT__REG         BMA250_INT_ENABLE1_REG

#define BMA250_EN_SLOPE_XYZ_INT__POS       0
#define BMA250_EN_SLOPE_XYZ_INT__LEN       3
#define BMA250_EN_SLOPE_XYZ_INT__MSK       0x07
#define BMA250_EN_SLOPE_XYZ_INT__REG       BMA250_INT_ENABLE1_REG

#define BMA250_EN_DOUBLE_TAP_INT__POS      4
#define BMA250_EN_DOUBLE_TAP_INT__LEN      1
#define BMA250_EN_DOUBLE_TAP_INT__MSK      0x10
#define BMA250_EN_DOUBLE_TAP_INT__REG      BMA250_INT_ENABLE1_REG

#define BMA250_EN_SINGLE_TAP_INT__POS      5
#define BMA250_EN_SINGLE_TAP_INT__LEN      1
#define BMA250_EN_SINGLE_TAP_INT__MSK      0x20
#define BMA250_EN_SINGLE_TAP_INT__REG      BMA250_INT_ENABLE1_REG

#define BMA250_EN_ORIENT_INT__POS          6
#define BMA250_EN_ORIENT_INT__LEN          1
#define BMA250_EN_ORIENT_INT__MSK          0x40
#define BMA250_EN_ORIENT_INT__REG          BMA250_INT_ENABLE1_REG

#define BMA250_EN_FLAT_INT__POS            7
#define BMA250_EN_FLAT_INT__LEN            1
#define BMA250_EN_FLAT_INT__MSK            0x80
#define BMA250_EN_FLAT_INT__REG            BMA250_INT_ENABLE1_REG

#define BMA250_EN_HIGHG_X_INT__POS         0
#define BMA250_EN_HIGHG_X_INT__LEN         1
#define BMA250_EN_HIGHG_X_INT__MSK         0x01
#define BMA250_EN_HIGHG_X_INT__REG         BMA250_INT_ENABLE2_REG

#define BMA250_EN_HIGHG_Y_INT__POS         1
#define BMA250_EN_HIGHG_Y_INT__LEN         1
#define BMA250_EN_HIGHG_Y_INT__MSK         0x02
#define BMA250_EN_HIGHG_Y_INT__REG         BMA250_INT_ENABLE2_REG

#define BMA250_EN_HIGHG_Z_INT__POS         2
#define BMA250_EN_HIGHG_Z_INT__LEN         1
#define BMA250_EN_HIGHG_Z_INT__MSK         0x04
#define BMA250_EN_HIGHG_Z_INT__REG         BMA250_INT_ENABLE2_REG

#define BMA250_EN_HIGHG_XYZ_INT__POS       2
#define BMA250_EN_HIGHG_XYZ_INT__LEN       1
#define BMA250_EN_HIGHG_XYZ_INT__MSK       0x04
#define BMA250_EN_HIGHG_XYZ_INT__REG       BMA250_INT_ENABLE2_REG

#define BMA250_EN_LOWG_INT__POS            3
#define BMA250_EN_LOWG_INT__LEN            1
#define BMA250_EN_LOWG_INT__MSK            0x08
#define BMA250_EN_LOWG_INT__REG            BMA250_INT_ENABLE2_REG

#define BMA250_EN_NEW_DATA_INT__POS        4
#define BMA250_EN_NEW_DATA_INT__LEN        1
#define BMA250_EN_NEW_DATA_INT__MSK        0x10
#define BMA250_EN_NEW_DATA_INT__REG        BMA250_INT_ENABLE2_REG

#define BMA250_EN_INT1_PAD_LOWG__POS       0
#define BMA250_EN_INT1_PAD_LOWG__LEN       1
#define BMA250_EN_INT1_PAD_LOWG__MSK       0x01
#define BMA250_EN_INT1_PAD_LOWG__REG       BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_HIGHG__POS      1
#define BMA250_EN_INT1_PAD_HIGHG__LEN      1
#define BMA250_EN_INT1_PAD_HIGHG__MSK      0x02
#define BMA250_EN_INT1_PAD_HIGHG__REG      BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_SLOPE__POS      2
#define BMA250_EN_INT1_PAD_SLOPE__LEN      1
#define BMA250_EN_INT1_PAD_SLOPE__MSK      0x04
#define BMA250_EN_INT1_PAD_SLOPE__REG      BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_DB_TAP__POS     4
#define BMA250_EN_INT1_PAD_DB_TAP__LEN     1
#define BMA250_EN_INT1_PAD_DB_TAP__MSK     0x10
#define BMA250_EN_INT1_PAD_DB_TAP__REG     BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_SNG_TAP__POS    5
#define BMA250_EN_INT1_PAD_SNG_TAP__LEN    1
#define BMA250_EN_INT1_PAD_SNG_TAP__MSK    0x20
#define BMA250_EN_INT1_PAD_SNG_TAP__REG    BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_ORIENT__POS     6
#define BMA250_EN_INT1_PAD_ORIENT__LEN     1
#define BMA250_EN_INT1_PAD_ORIENT__MSK     0x40
#define BMA250_EN_INT1_PAD_ORIENT__REG     BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_FLAT__POS       7
#define BMA250_EN_INT1_PAD_FLAT__LEN       1
#define BMA250_EN_INT1_PAD_FLAT__MSK       0x80
#define BMA250_EN_INT1_PAD_FLAT__REG       BMA250_INT1_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_LOWG__POS       0
#define BMA250_EN_INT2_PAD_LOWG__LEN       1
#define BMA250_EN_INT2_PAD_LOWG__MSK       0x01
#define BMA250_EN_INT2_PAD_LOWG__REG       BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_HIGHG__POS      1
#define BMA250_EN_INT2_PAD_HIGHG__LEN      1
#define BMA250_EN_INT2_PAD_HIGHG__MSK      0x02
#define BMA250_EN_INT2_PAD_HIGHG__REG      BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_SLOPE__POS      2
#define BMA250_EN_INT2_PAD_SLOPE__LEN      1
#define BMA250_EN_INT2_PAD_SLOPE__MSK      0x04
#define BMA250_EN_INT2_PAD_SLOPE__REG      BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_DB_TAP__POS     4
#define BMA250_EN_INT2_PAD_DB_TAP__LEN     1
#define BMA250_EN_INT2_PAD_DB_TAP__MSK     0x10
#define BMA250_EN_INT2_PAD_DB_TAP__REG     BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_SNG_TAP__POS    5
#define BMA250_EN_INT2_PAD_SNG_TAP__LEN    1
#define BMA250_EN_INT2_PAD_SNG_TAP__MSK    0x20
#define BMA250_EN_INT2_PAD_SNG_TAP__REG    BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_ORIENT__POS     6
#define BMA250_EN_INT2_PAD_ORIENT__LEN     1
#define BMA250_EN_INT2_PAD_ORIENT__MSK     0x40
#define BMA250_EN_INT2_PAD_ORIENT__REG     BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT2_PAD_FLAT__POS       7
#define BMA250_EN_INT2_PAD_FLAT__LEN       1
#define BMA250_EN_INT2_PAD_FLAT__MSK       0x80
#define BMA250_EN_INT2_PAD_FLAT__REG       BMA250_INT2_PAD_SEL_REG

#define BMA250_EN_INT1_PAD_NEWDATA__POS    0
#define BMA250_EN_INT1_PAD_NEWDATA__LEN    1
#define BMA250_EN_INT1_PAD_NEWDATA__MSK    0x01
#define BMA250_EN_INT1_PAD_NEWDATA__REG    BMA250_INT_DATA_SEL_REG

#define BMA250_EN_INT2_PAD_NEWDATA__POS    7
#define BMA250_EN_INT2_PAD_NEWDATA__LEN    1
#define BMA250_EN_INT2_PAD_NEWDATA__MSK    0x80
#define BMA250_EN_INT2_PAD_NEWDATA__REG    BMA250_INT_DATA_SEL_REG


#define BMA250_UNFILT_INT_SRC_LOWG__POS    0
#define BMA250_UNFILT_INT_SRC_LOWG__LEN    1
#define BMA250_UNFILT_INT_SRC_LOWG__MSK    0x01
#define BMA250_UNFILT_INT_SRC_LOWG__REG    BMA250_INT_SRC_REG

#define BMA250_UNFILT_INT_SRC_HIGHG__POS   1
#define BMA250_UNFILT_INT_SRC_HIGHG__LEN   1
#define BMA250_UNFILT_INT_SRC_HIGHG__MSK   0x02
#define BMA250_UNFILT_INT_SRC_HIGHG__REG   BMA250_INT_SRC_REG

#define BMA250_UNFILT_INT_SRC_SLOPE__POS   2
#define BMA250_UNFILT_INT_SRC_SLOPE__LEN   1
#define BMA250_UNFILT_INT_SRC_SLOPE__MSK   0x04
#define BMA250_UNFILT_INT_SRC_SLOPE__REG   BMA250_INT_SRC_REG

#define BMA250_UNFILT_INT_SRC_TAP__POS     4
#define BMA250_UNFILT_INT_SRC_TAP__LEN     1
#define BMA250_UNFILT_INT_SRC_TAP__MSK     0x10
#define BMA250_UNFILT_INT_SRC_TAP__REG     BMA250_INT_SRC_REG

#define BMA250_UNFILT_INT_SRC_DATA__POS    5
#define BMA250_UNFILT_INT_SRC_DATA__LEN    1
#define BMA250_UNFILT_INT_SRC_DATA__MSK    0x20
#define BMA250_UNFILT_INT_SRC_DATA__REG    BMA250_INT_SRC_REG

#define BMA250_INT1_PAD_ACTIVE_LEVEL__POS  0
#define BMA250_INT1_PAD_ACTIVE_LEVEL__LEN  1
#define BMA250_INT1_PAD_ACTIVE_LEVEL__MSK  0x01
#define BMA250_INT1_PAD_ACTIVE_LEVEL__REG  BMA250_INT_SET_REG

#define BMA250_INT2_PAD_ACTIVE_LEVEL__POS  2
#define BMA250_INT2_PAD_ACTIVE_LEVEL__LEN  1
#define BMA250_INT2_PAD_ACTIVE_LEVEL__MSK  0x04
#define BMA250_INT2_PAD_ACTIVE_LEVEL__REG  BMA250_INT_SET_REG

#define BMA250_INT1_PAD_OUTPUT_TYPE__POS   1
#define BMA250_INT1_PAD_OUTPUT_TYPE__LEN   1
#define BMA250_INT1_PAD_OUTPUT_TYPE__MSK   0x02
#define BMA250_INT1_PAD_OUTPUT_TYPE__REG   BMA250_INT_SET_REG

#define BMA250_INT2_PAD_OUTPUT_TYPE__POS   3
#define BMA250_INT2_PAD_OUTPUT_TYPE__LEN   1 
#define BMA250_INT2_PAD_OUTPUT_TYPE__MSK   0x08
#define BMA250_INT2_PAD_OUTPUT_TYPE__REG   BMA250_INT_SET_REG


#define BMA250_INT_MODE_SEL__POS           0
#define BMA250_INT_MODE_SEL__LEN           4
#define BMA250_INT_MODE_SEL__MSK           0x0F
#define BMA250_INT_MODE_SEL__REG           BMA250_INT_CTRL_REG


#define BMA250_INT_RESET_LATCHED__POS      7
#define BMA250_INT_RESET_LATCHED__LEN      1
#define BMA250_INT_RESET_LATCHED__MSK      0x80
#define BMA250_INT_RESET_LATCHED__REG      BMA250_INT_CTRL_REG

#define BMA250_LOWG_DUR__POS               0
#define BMA250_LOWG_DUR__LEN               8
#define BMA250_LOWG_DUR__MSK               0xFF
#define BMA250_LOWG_DUR__REG               BMA250_LOW_DURN_REG

#define BMA250_LOWG_THRES__POS             0
#define BMA250_LOWG_THRES__LEN             8
#define BMA250_LOWG_THRES__MSK             0xFF
#define BMA250_LOWG_THRES__REG             BMA250_LOW_THRES_REG

#define BMA250_LOWG_HYST__POS              0
#define BMA250_LOWG_HYST__LEN              2
#define BMA250_LOWG_HYST__MSK              0x03
#define BMA250_LOWG_HYST__REG              BMA250_LOW_HIGH_HYST_REG

#define BMA250_LOWG_INT_MODE__POS          2
#define BMA250_LOWG_INT_MODE__LEN          1
#define BMA250_LOWG_INT_MODE__MSK          0x04
#define BMA250_LOWG_INT_MODE__REG          BMA250_LOW_HIGH_HYST_REG

#define BMA250_HIGHG_DUR__POS              0
#define BMA250_HIGHG_DUR__LEN              8
#define BMA250_HIGHG_DUR__MSK              0xFF
#define BMA250_HIGHG_DUR__REG              BMA250_HIGH_DURN_REG

#define BMA250_HIGHG_THRES__POS            0
#define BMA250_HIGHG_THRES__LEN            8
#define BMA250_HIGHG_THRES__MSK            0xFF
#define BMA250_HIGHG_THRES__REG            BMA250_HIGH_THRES_REG

#define BMA250_HIGHG_HYST__POS             6
#define BMA250_HIGHG_HYST__LEN             2
#define BMA250_HIGHG_HYST__MSK             0xC0
#define BMA250_HIGHG_HYST__REG             BMA250_LOW_HIGH_HYST_REG

#define BMA250_SLOPE_DUR__POS              0
#define BMA250_SLOPE_DUR__LEN              2
#define BMA250_SLOPE_DUR__MSK              0x03
#define BMA250_SLOPE_DUR__REG              BMA250_SLOPE_DURN_REG

#define BMA250_SLOPE_THRES__POS            0
#define BMA250_SLOPE_THRES__LEN            8
#define BMA250_SLOPE_THRES__MSK            0xFF
#define BMA250_SLOPE_THRES__REG            BMA250_SLOPE_THRES_REG

#define BMA250_TAP_DUR__POS                0
#define BMA250_TAP_DUR__LEN                3
#define BMA250_TAP_DUR__MSK                0x07
#define BMA250_TAP_DUR__REG                BMA250_TAP_PARAM_REG

#define BMA250_TAP_SHOCK_DURN__POS         6
#define BMA250_TAP_SHOCK_DURN__LEN         1
#define BMA250_TAP_SHOCK_DURN__MSK         0x40
#define BMA250_TAP_SHOCK_DURN__REG         BMA250_TAP_PARAM_REG

#define BMA250_TAP_QUIET_DURN__POS         7
#define BMA250_TAP_QUIET_DURN__LEN         1
#define BMA250_TAP_QUIET_DURN__MSK         0x80
#define BMA250_TAP_QUIET_DURN__REG         BMA250_TAP_PARAM_REG

#define BMA250_TAP_THRES__POS              0
#define BMA250_TAP_THRES__LEN              5
#define BMA250_TAP_THRES__MSK              0x1F
#define BMA250_TAP_THRES__REG              BMA250_TAP_THRES_REG

#define BMA250_TAP_SAMPLES__POS            6
#define BMA250_TAP_SAMPLES__LEN            2
#define BMA250_TAP_SAMPLES__MSK            0xC0
#define BMA250_TAP_SAMPLES__REG            BMA250_TAP_THRES_REG

#define BMA250_ORIENT_MODE__POS            0
#define BMA250_ORIENT_MODE__LEN            2
#define BMA250_ORIENT_MODE__MSK            0x03
#define BMA250_ORIENT_MODE__REG            BMA250_ORIENT_PARAM_REG

#define BMA250_ORIENT_BLOCK__POS           2
#define BMA250_ORIENT_BLOCK__LEN           2
#define BMA250_ORIENT_BLOCK__MSK           0x0C
#define BMA250_ORIENT_BLOCK__REG           BMA250_ORIENT_PARAM_REG

#define BMA250_ORIENT_HYST__POS            4
#define BMA250_ORIENT_HYST__LEN            3
#define BMA250_ORIENT_HYST__MSK            0x70
#define BMA250_ORIENT_HYST__REG            BMA250_ORIENT_PARAM_REG

#define BMA250_ORIENT_AXIS__POS            7
#define BMA250_ORIENT_AXIS__LEN            1
#define BMA250_ORIENT_AXIS__MSK            0x80
#define BMA250_ORIENT_AXIS__REG            BMA250_THETA_BLOCK_REG

#define BMA250_THETA_BLOCK__POS            0
#define BMA250_THETA_BLOCK__LEN            6
#define BMA250_THETA_BLOCK__MSK            0x3F
#define BMA250_THETA_BLOCK__REG            BMA250_THETA_BLOCK_REG

#define BMA250_THETA_FLAT__POS             0
#define BMA250_THETA_FLAT__LEN             6
#define BMA250_THETA_FLAT__MSK             0x3F
#define BMA250_THETA_FLAT__REG             BMA250_THETA_FLAT_REG

#define BMA250_FLAT_HOLD_TIME__POS         4
#define BMA250_FLAT_HOLD_TIME__LEN         2
#define BMA250_FLAT_HOLD_TIME__MSK         0x30
#define BMA250_FLAT_HOLD_TIME__REG         BMA250_FLAT_HOLD_TIME_REG

#define BMA250_LOW_POWER_MODE_S__POS       0
#define BMA250_LOW_POWER_MODE_S__LEN       1
#define BMA250_LOW_POWER_MODE_S__MSK       0x01
#define BMA250_LOW_POWER_MODE_S__REG       BMA250_STATUS_LOW_POWER_REG


/* add for fast calibration*/
#define BMA250_EN_FAST_COMP__POS           5
#define BMA250_EN_FAST_COMP__LEN           2
#define BMA250_EN_FAST_COMP__MSK           0x60
#define BMA250_EN_FAST_COMP__REG           BMA250_OFFSET_CTRL_REG

#define BMA250_FAST_COMP_RDY_S__POS        4
#define BMA250_FAST_COMP_RDY_S__LEN        1
#define BMA250_FAST_COMP_RDY_S__MSK        0x10
#define BMA250_FAST_COMP_RDY_S__REG        BMA250_OFFSET_CTRL_REG

#define BMA250_NVM_RDY__POS                2
#define BMA250_NVM_RDY__LEN                1
#define BMA250_NVM_RDY__MSK                0x04
#define BMA250_NVM_RDY__REG                BMA250_EEPROM_CTRL_REG

#define BMA250_NVM_PROG_TRIG__POS          1
#define BMA250_NVM_PROG_TRIG__LEN          1
#define BMA250_NVM_PROG_TRIG__MSK          0x02
#define BMA250_NVM_PROG_TRIG__REG          BMA250_EEPROM_CTRL_REG

#define BMA250_NVM_PROG_MODE__POS          0
#define BMA250_NVM_PROG_MODE__LEN          1
#define BMA250_NVM_PROG_MODE__MSK          0x01
#define BMA250_NVM_PROG_MODE__REG          BMA250_EEPROM_CTRL_REG


#define BMA250_COMP_TARGET_OFFSET_X__POS   1
#define BMA250_COMP_TARGET_OFFSET_X__LEN   2
#define BMA250_COMP_TARGET_OFFSET_X__MSK   0x06
#define BMA250_COMP_TARGET_OFFSET_X__REG   BMA250_OFFSET_PARAMS_REG

#define BMA250_COMP_TARGET_OFFSET_Y__POS   3
#define BMA250_COMP_TARGET_OFFSET_Y__LEN   2
#define BMA250_COMP_TARGET_OFFSET_Y__MSK   0x18
#define BMA250_COMP_TARGET_OFFSET_Y__REG   BMA250_OFFSET_PARAMS_REG

#define BMA250_COMP_TARGET_OFFSET_Z__POS   5
#define BMA250_COMP_TARGET_OFFSET_Z__LEN   2
#define BMA250_COMP_TARGET_OFFSET_Z__MSK   0x60
#define BMA250_COMP_TARGET_OFFSET_Z__REG   BMA250_OFFSET_PARAMS_REG



#define BMA250_GET_BITSLICE(regvar, bitname)\
			((regvar & bitname##__MSK) >> bitname##__POS)


#define BMA250_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))


/* range and bandwidth */

#define BMA250_RANGE_2G         0
#define BMA250_RANGE_4G         1
#define BMA250_RANGE_8G         2
#define BMA250_RANGE_16G        3


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

/* self test*/
#define BMA250_SELFTEST_X	1
#define BMA250_SELFTEST_Y	2
#define BMA250_SELFTEST_Z	3


struct bma250acc{
	s16	x,
		y,
		z;
} ;

struct bma250_data {
	struct i2c_client *bma250_client;
	atomic_t delay;
	atomic_t enable;
	unsigned char mode;
	struct input_dev *input;
	struct bma250acc reported_value;
	unsigned int report_threshold; /* controls reporting to upper layer */
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct delayed_work work;
	struct work_struct irq_work;
	struct regulator *vdd_regulator;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int IRQ1;
	int IRQ2;
};

static int bma250_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -1;
	*data = dummy & 0x000000ff;

	return 0;
}

static int bma250_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma250_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma250_get_cal_ready(struct i2c_client *client, unsigned char *calrdy )
{
	int comres = 0 ;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_OFFSET_CTRL_REG, &data);
	data = BMA250_GET_BITSLICE(data, BMA250_FAST_COMP_RDY_S);
	*calrdy = data;

	return comres;
}

static int bma250_set_cal_trigger(struct i2c_client *client, unsigned char caltrigger)
{
	int comres=0;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_EN_FAST_COMP__REG, &data);
	data = BMA250_SET_BITSLICE(data, BMA250_EN_FAST_COMP, caltrigger );
	comres = bma250_smbus_write_byte(client, BMA250_EN_FAST_COMP__REG, &data);

	return comres;
}

static int bma250_write_reg(struct i2c_client *client, unsigned char addr, unsigned char *data)
{
	int comres = 0 ;
	comres = bma250_smbus_write_byte(client, addr, data);

	return comres;
}


static int bma250_set_offset_target_x(struct i2c_client *client, unsigned char offsettarget)
{
	int comres=0;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_COMP_TARGET_OFFSET_X__REG, &data);
	data = BMA250_SET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_X, offsettarget );
	comres = bma250_smbus_write_byte(client, BMA250_COMP_TARGET_OFFSET_X__REG, &data);

	return comres;
}

static int bma250_write_eeprom(struct i2c_client *client)
{
	int comres=0;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	data = BMA250_SET_BITSLICE(data, BMA250_NVM_PROG_MODE, 1 );
	comres = bma250_smbus_write_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	data = BMA250_SET_BITSLICE(data, BMA250_NVM_PROG_TRIG, 1 );
	comres = bma250_smbus_write_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	do {
	mdelay(1);
	comres = bma250_smbus_read_byte(client, BMA250_EEPROM_CTRL_REG, &data);	
	}
	while(!BMA250_GET_BITSLICE(data, BMA250_NVM_RDY));

	data = BMA250_SET_BITSLICE(data, BMA250_NVM_PROG_MODE, 0 );
	comres = bma250_smbus_write_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	return comres;
}

static int bma250_get_offset_target_x(struct i2c_client *client, unsigned char *offsettarget )
{
	int comres = 0 ;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_OFFSET_PARAMS_REG, &data);
	data = BMA250_GET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_X);
	*offsettarget = data;

	return comres;
}

static int bma250_set_offset_target_y(struct i2c_client *client, unsigned char offsettarget)
{
	int comres=0;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_COMP_TARGET_OFFSET_Y__REG, &data);
	data = BMA250_SET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Y, offsettarget );
	comres = bma250_smbus_write_byte(client, BMA250_COMP_TARGET_OFFSET_Y__REG, &data);

	return comres;
}

static int bma250_get_offset_target_y(struct i2c_client *client, unsigned char *offsettarget )
{
	int comres = 0 ;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_OFFSET_PARAMS_REG, &data);
	data = BMA250_GET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Y);
	*offsettarget = data;

	return comres;
}

static int bma250_set_offset_target_z(struct i2c_client *client, unsigned char offsettarget)
{
	int comres=0;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_COMP_TARGET_OFFSET_Z__REG, &data);
	data = BMA250_SET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Z, offsettarget );
	comres = bma250_smbus_write_byte(client, BMA250_COMP_TARGET_OFFSET_Z__REG, &data);

	return comres;
}

static int bma250_get_offset_target_z(struct i2c_client *client, unsigned char *offsettarget )
{
	int comres = 0 ;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, BMA250_OFFSET_PARAMS_REG, &data);
	data = BMA250_GET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Z);
	*offsettarget = data;

	return comres;
}

static int bma250_get_filt(struct i2c_client *client, unsigned char *filt, unsigned char axis)
{
	int comres = 0 ;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, (BMA250_OFFSET_FILT_X_REG + axis), &data);
	*filt = data;
	return comres;
}

static int bma250_get_unfilt(struct i2c_client *client, unsigned char *unfilt, unsigned char axis)
{
	int comres = 0 ;
	unsigned char data;

	comres = bma250_smbus_read_byte(client, (BMA250_OFFSET_UNFILT_X_REG + axis), &data);
	*unfilt = data;
	return comres;
}

static int bma250_set_mode(struct i2c_client *client, unsigned char Mode)
{
	int comres = 0;
	unsigned char data1;

	if (client == NULL) {
		comres = -1;
	} else{
		if (Mode < 3) {
			comres = bma250_smbus_read_byte(client,
					BMA250_EN_LOW_POWER__REG, &data1);
			switch (Mode) {
			case BMA250_MODE_NORMAL:
				data1  = BMA250_SET_BITSLICE(data1,
					BMA250_EN_LOW_POWER, 0);
				data1  = BMA250_SET_BITSLICE(data1,
					BMA250_EN_SUSPEND, 0);
				break;
			case BMA250_MODE_LOWPOWER:
				data1  = BMA250_SET_BITSLICE(data1,
					BMA250_EN_LOW_POWER, 1);
				data1  = BMA250_SET_BITSLICE(data1,
					BMA250_EN_SUSPEND, 0);
				break;
			case BMA250_MODE_SUSPEND:
				data1  = BMA250_SET_BITSLICE(data1,
					BMA250_EN_LOW_POWER, 0);
				data1  = BMA250_SET_BITSLICE(data1,
					BMA250_EN_SUSPEND, 1);
				break;
			default:
				break;
			}

			comres += bma250_smbus_write_byte(client,
					BMA250_EN_LOW_POWER__REG, &data1);
		} else{
			comres = -1;
		}
	}

	return comres;
}

static int bma250_get_mode(struct i2c_client *client, unsigned char *Mode)
{
	int comres = 0;

	if (client == NULL) {
		comres = -1;
	} else{
		comres = bma250_smbus_read_byte(client,
				BMA250_EN_LOW_POWER__REG, Mode);
		*Mode  = (*Mode) >> 6;
	}

	return comres;
}


static void bma250_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&bma250->enable);

	mutex_lock(&bma250->enable_mutex);
	if (enable) {
		if (pre_enable ==0) {
			bma250_set_mode(bma250->bma250_client,
					BMA250_MODE_NORMAL);
			schedule_delayed_work(&bma250->work,
					msecs_to_jiffies(atomic_read(&bma250->delay)));
			atomic_set(&bma250->enable, 1);
		}

	} else {
		if (pre_enable ==1) {
			bma250_set_mode(bma250->bma250_client,
					BMA250_MODE_SUSPEND);
			cancel_delayed_work_sync(&bma250->work);
			atomic_set(&bma250->enable, 0);
		}
	}
	mutex_unlock(&bma250->enable_mutex);

}




static int bma250_set_range(struct i2c_client *client, unsigned char Range)
{
	int comres = 0;
	unsigned char data1;

	if (client == NULL) {
		comres = -1;
	} else{
		if (Range < 4) {
			comres = bma250_smbus_read_byte(client,
					BMA250_RANGE_SEL_REG, &data1);
			switch (Range) {
			case 0:
				data1  = BMA250_SET_BITSLICE(data1,
						BMA250_RANGE_SEL, 0);
				break;
			case 1:
				data1  = BMA250_SET_BITSLICE(data1,
						BMA250_RANGE_SEL, 5);
				break;
			case 2:
				data1  = BMA250_SET_BITSLICE(data1,
						BMA250_RANGE_SEL, 8);
				break;
			case 3:
				data1  = BMA250_SET_BITSLICE(data1,
						BMA250_RANGE_SEL, 12);
				break;
			default:
					break;
			}
			comres += bma250_smbus_write_byte(client,
					BMA250_RANGE_SEL_REG, &data1);
		} else{
			comres = -1;
		}
	}

	return comres;
}

static int bma250_get_range(struct i2c_client *client, unsigned char *Range)
{
	int comres = 0;
	unsigned char data;

	if (client == NULL) {
		comres = -1;
	} else{
		comres = bma250_smbus_read_byte(client, BMA250_RANGE_SEL__REG,
				&data);
		data = BMA250_GET_BITSLICE(data, BMA250_RANGE_SEL);
		*Range = data;
	}

	return comres;
}


static int bma250_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data;
	int Bandwidth = 0;

	if (client == NULL) {
		comres = -1;
	} else{
		if (BW < 8) {
			switch (BW) {
			case 0:
				Bandwidth = BMA250_BW_7_81HZ;
				break;
			case 1:
				Bandwidth = BMA250_BW_15_63HZ;
				break;
			case 2:
				Bandwidth = BMA250_BW_31_25HZ;
				break;
			case 3:
				Bandwidth = BMA250_BW_62_50HZ;
				break;
			case 4:
				Bandwidth = BMA250_BW_125HZ;
				break;
			case 5:
				Bandwidth = BMA250_BW_250HZ;
				break;
			case 6:
				Bandwidth = BMA250_BW_500HZ;
				break;
			case 7:
				Bandwidth = BMA250_BW_1000HZ;
				break;
			default:
					break;
			}
			comres = bma250_smbus_read_byte(client,
					BMA250_BANDWIDTH__REG, &data);
			data = BMA250_SET_BITSLICE(data, BMA250_BANDWIDTH,
					Bandwidth);
			comres += bma250_smbus_write_byte(client,
					BMA250_BANDWIDTH__REG, &data);
		} else{
			comres = -1;
		}
	}

	return comres;
}

static int bma250_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data;

	if (client == NULL) {
		comres = -1;
	} else{
		comres = bma250_smbus_read_byte(client, BMA250_BANDWIDTH__REG,
				&data);
		data = BMA250_GET_BITSLICE(data, BMA250_BANDWIDTH);
		if (data <= 8) {
			*BW = 0;
		} else{
			if (data >= 0x0F)
				*BW = 7;
			else
				*BW = data - 8;

		}
	}

	return comres;
}

static int bma250_set_selftest(struct i2c_client *client, unsigned char AXIS, unsigned char SIGN)
{
	int comres = 0;
	unsigned char data = 0;
	int Axis = 0;

	if (client == NULL) {
		comres = -1;
	} else{
			switch (AXIS) {
			case 'x':
				Axis = BMA250_SELFTEST_X;
				data = Axis | (SIGN<<2);
				break;
			case 'y':
				Axis = BMA250_SELFTEST_Y;
				data = Axis | (SIGN<<2);
				break;
			case 'z':
				Axis = BMA250_SELFTEST_Z;
				data = Axis | (SIGN<<2);
				break;
			default:
				data = 0;
				break;
			}
			comres = bma250_smbus_write_byte(client,
					BMA250_SELF_TEST_REG, &data);
	}

	return comres;
}

static int bma250_read_accel_xyz(struct i2c_client *client,
							struct bma250acc *acc)
{
	int comres;
	s16 temp_x;
	unsigned char data[6];
	if (client == NULL) {
		comres = -1;
	} else{
		comres = bma250_smbus_read_byte_block(client,
				BMA250_ACC_X_LSB__REG, data, 6);

		acc->x = BMA250_GET_BITSLICE(data[0], BMA250_ACC_X_LSB)
			|(BMA250_GET_BITSLICE(data[1],
				BMA250_ACC_X_MSB)<<BMA250_ACC_X_LSB__LEN);
		acc->x = acc->x << (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN
					+ BMA250_ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN
					+ BMA250_ACC_X_MSB__LEN));
		acc->y = BMA250_GET_BITSLICE(data[2], BMA250_ACC_Y_LSB)
			| (BMA250_GET_BITSLICE(data[3],
				BMA250_ACC_Y_MSB)<<BMA250_ACC_Y_LSB__LEN);
		acc->y = acc->y << (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN
					+ BMA250_ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN
					+ BMA250_ACC_Y_MSB__LEN));

		acc->z = BMA250_GET_BITSLICE(data[4], BMA250_ACC_Z_LSB)
			| (BMA250_GET_BITSLICE(data[5],
				BMA250_ACC_Z_MSB)<<BMA250_ACC_Z_LSB__LEN);
		acc->z = acc->z << (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN
					+ BMA250_ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN
					+ BMA250_ACC_Z_MSB__LEN));
	}

		if(quanta_get_mbid()<2) {
		temp_x = acc->x;
		acc->x = -acc->y;
		acc->y = -temp_x;
		acc->z = -acc->z;
		}
		else {
		temp_x = acc->x;
		acc->x = acc->y;
		acc->y = temp_x;
		acc->z = -acc->z;
		}

	return comres;
}

static void bma250_work_func(struct work_struct *work)
{
	struct bma250_data *bma250 = container_of((struct delayed_work *)work,
			struct bma250_data, work);
	static struct bma250acc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&bma250->delay));

	bma250_read_accel_xyz(bma250->bma250_client, &acc);

	if ((abs(acc.x - bma250->reported_value.x) >= bma250->report_threshold) ||
	    (abs(acc.y - bma250->reported_value.y) >= bma250->report_threshold) ||
	    (abs(acc.z - bma250->reported_value.z) >= bma250->report_threshold)) {
		input_report_abs(bma250->input, ABS_X, acc.x);
		input_report_abs(bma250->input, ABS_Y, acc.y);
		input_report_abs(bma250->input, ABS_Z, acc.z);
		input_sync(bma250->input);
		mutex_lock(&bma250->value_mutex);
		bma250->reported_value = acc;
		mutex_unlock(&bma250->value_mutex);
//		printk("report: %d %d %d\n", acc.x, acc.y, acc.z);
	}
	schedule_delayed_work(&bma250->work, delay);
}


static ssize_t bma250_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &address, &value);

	if (bma250_write_reg(bma250->bma250_client, (unsigned char)address, (unsigned char *)&value) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma250_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x3d];
	int i;

	for(i =0; i < 0x3d; i++) {
		bma250_smbus_read_byte(bma250->bma250_client,i,reg+i);

		count += sprintf(&buf[count], "0x%x: %d\n", i, reg[i]);
	}
	return count;


}
static ssize_t bma250_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_range(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma250_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma250_set_range(bma250->bma250_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma250_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_bandwidth(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma250_bandwidth_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma250_set_bandwidth(bma250->bma250_client,
						 (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma250_report_threshold_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        unsigned int data;
        struct i2c_client *client = to_i2c_client(dev);
        struct bma250_data *bma250 = i2c_get_clientdata(client);

	data = bma250->report_threshold;

        return sprintf(buf, "%d\n", data);

}

static ssize_t bma250_report_threshold_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
        unsigned long data;
        int error;
        struct i2c_client *client = to_i2c_client(dev);
        struct bma250_data *bma250 = i2c_get_clientdata(client);

        error = strict_strtoul(buf, 10, &data);
        if (error)
                return error;

	bma250->report_threshold = data;

        return count;
}


static ssize_t bma250_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_mode(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma250_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma250_set_mode(bma250->bma250_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}


static ssize_t bma250_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma250_data *bma250 = input_get_drvdata(input);
	struct bma250acc acc_value;

	mutex_lock(&bma250->value_mutex);
	acc_value = bma250->reported_value;
	mutex_unlock(&bma250->value_mutex);

	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t bma250_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma250->delay));

}

static ssize_t bma250_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (data > BMA250_MAX_DELAY)
		data = BMA250_MAX_DELAY;
	atomic_set(&bma250->delay, (unsigned int) data);

	return count;
}


static ssize_t bma250_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char value[2];
	s16 temp;
	int error;
	long positive_values = 0;
	long negative_values = 0;
	long delta = 0;
	unsigned char i;
	struct i2c_client *client =  to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);
		error = bma250_set_selftest(bma250->bma250_client, 'x', 0);
		mdelay(10);
		for (i =0; i< 15; i++) {		
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client,
				BMA250_ACC_X_LSB__REG, value, 2);

		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_X_LSB)
			|(BMA250_GET_BITSLICE(value[1],
				BMA250_ACC_X_MSB)<<BMA250_ACC_X_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN
					+ BMA250_ACC_X_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN
					+ BMA250_ACC_X_MSB__LEN));
		if (i >=5 )
		positive_values += temp;
		}
		positive_values /= 10;
		error = bma250_set_selftest(bma250->bma250_client, 'x', 1);
		mdelay(10);
		for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client,
				BMA250_ACC_X_LSB__REG, value, 2);

		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_X_LSB)
			|(BMA250_GET_BITSLICE(value[1],
				BMA250_ACC_X_MSB)<<BMA250_ACC_X_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN
					+ BMA250_ACC_X_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN
					+ BMA250_ACC_X_MSB__LEN));
		if (i >=5 )
		negative_values += temp;
		}
		negative_values /= 10;
		delta = positive_values - negative_values;
		error = bma250_set_selftest(bma250->bma250_client, 'a', 0);
		if (delta < 205)
			return sprintf(buf, "Fail\n");		
		error = bma250_set_selftest(bma250->bma250_client, 'y', 0);
		mdelay(10);
		for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client,
				BMA250_ACC_Y_LSB__REG, value, 2);

		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Y_LSB)
			|(BMA250_GET_BITSLICE(value[1],
				BMA250_ACC_Y_MSB)<<BMA250_ACC_Y_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN
					+ BMA250_ACC_Y_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN
					+ BMA250_ACC_Y_MSB__LEN));
		if (i >=5 )
		positive_values += temp;
		}
		positive_values /= 10;
		error = bma250_set_selftest(bma250->bma250_client, 'y', 1);
		mdelay(10);
		for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client,
				BMA250_ACC_Y_LSB__REG, value, 2);

		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Y_LSB)
			|(BMA250_GET_BITSLICE(value[1],
				BMA250_ACC_Y_MSB)<<BMA250_ACC_Y_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN
					+ BMA250_ACC_Y_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN
					+ BMA250_ACC_Y_MSB__LEN));
		if (i >=5 )
		negative_values += temp;
		}
		negative_values /= 10;
		delta = positive_values - negative_values;	
		error = bma250_set_selftest(bma250->bma250_client, 'a', 0);
		if (delta < 205)
			return sprintf(buf, "Fail\n");
		error = bma250_set_selftest(bma250->bma250_client, 'z', 0);
		mdelay(10);
		for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client,
				BMA250_ACC_Z_LSB__REG, value, 2);

		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Z_LSB)
			|(BMA250_GET_BITSLICE(value[1],
				BMA250_ACC_Z_MSB)<<BMA250_ACC_Z_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN
					+ BMA250_ACC_Z_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN
					+ BMA250_ACC_Z_MSB__LEN));
		if (i >=5 )
		positive_values += temp;
		}
		positive_values /= 10;
		error = bma250_set_selftest(bma250->bma250_client, 'z', 1);
		mdelay(10);
		for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client,
				BMA250_ACC_Z_LSB__REG, value, 2);

		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Z_LSB)
			|(BMA250_GET_BITSLICE(value[1],
				BMA250_ACC_Z_MSB)<<BMA250_ACC_Z_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN
					+ BMA250_ACC_Z_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN
					+ BMA250_ACC_Z_MSB__LEN));
		if (i >=5 )
		negative_values += temp;
		}
		negative_values /= 10;
		delta = positive_values - negative_values;		
		error = bma250_set_selftest(bma250->bma250_client, 'a', 0);
		if (delta <= 102)
			return sprintf(buf, "Fail\n");
	if (error < 0)
			return sprintf(buf, "Unknown Error\n");
	return sprintf(buf, "Pass\n");
}

static ssize_t bma250_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0)||(data==1)) {
		bma250_set_enable(dev,data);
	}

	return count;
}
static ssize_t bma250_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma250->enable));

}


static ssize_t bma250_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_offset_target_x(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma250_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma250_set_offset_target_x(bma250->bma250_client, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma250_set_cal_trigger(bma250->bma250_client, 1) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma250_get_cal_ready(bma250->bma250_client, &tmp);

		printk(KERN_INFO "wait 2ms and got cal ready flag is %d\n",tmp);
		timeout++;
		if(timeout==50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	}while(tmp==0);
	printk(KERN_INFO "x axis fast calibration finished\n");
	return count;
}

static ssize_t bma250_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_offset_target_y(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma250_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma250_set_offset_target_y(bma250->bma250_client, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma250_set_cal_trigger(bma250->bma250_client, 2) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma250_get_cal_ready(bma250->bma250_client, &tmp);

		printk(KERN_INFO "wait 2ms and got cal ready flag is %d\n",tmp);
		timeout++;
		if(timeout==50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	}while(tmp==0);

	printk(KERN_INFO "y axis fast calibration finished\n");
	return count;
}

static ssize_t bma250_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_offset_target_z(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma250_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma250_set_offset_target_z(bma250->bma250_client, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma250_set_cal_trigger(bma250->bma250_client, 3) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma250_get_cal_ready(bma250->bma250_client, &tmp);

		printk(KERN_INFO "wait 2ms and got cal ready flag is %d\n",tmp);
		timeout++;
		if(timeout==50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	}while(tmp==0);

	printk(KERN_INFO "z axis fast calibration finished\n");
	return count;
}

static ssize_t bma250_eeprom_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data[6];
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_filt(bma250->bma250_client, data, 0) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_filt(bma250->bma250_client, (data+1), 1) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_filt(bma250->bma250_client, (data+2), 2) < 0)
		return sprintf(buf, "Read error\n");

	if (bma250_get_unfilt(bma250->bma250_client, (data+3), 0) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_unfilt(bma250->bma250_client, (data+4), 1) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_unfilt(bma250->bma250_client, (data+5), 2) < 0)
		return sprintf(buf, "Read error\n");
	return sprintf(buf, "FIL X:%d Y:%d Z:%d\nUNFIL: X:%d Y:%d Z:%d\n", data[0], data[1], data[2], data[3], data[4], data[5]);
}

static ssize_t bma250_eeprom_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	bma250_write_eeprom(bma250->bma250_client);
	return count;
}






static DEVICE_ATTR(range, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_range_show, bma250_range_store);
static DEVICE_ATTR(bandwidth, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_bandwidth_show, bma250_bandwidth_store);
static DEVICE_ATTR(report_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
                bma250_report_threshold_show, bma250_report_threshold_store);
static DEVICE_ATTR(mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_mode_show, bma250_mode_store);
static DEVICE_ATTR(value, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_delay_show, bma250_delay_store);
static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_selftest_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_enable_show, bma250_enable_store);
static DEVICE_ATTR(reg, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_register_show, bma250_register_store);
static DEVICE_ATTR(fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_fast_calibration_x_show, bma250_fast_calibration_x_store);
static DEVICE_ATTR(fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_fast_calibration_y_show, bma250_fast_calibration_y_store);
static DEVICE_ATTR(fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_fast_calibration_z_show, bma250_fast_calibration_z_store);
static DEVICE_ATTR(eeprom, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_eeprom_show, bma250_eeprom_store);
static struct attribute *bma250_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_report_threshold.attr,
	&dev_attr_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_selftest.attr,
	&dev_attr_enable.attr,
	&dev_attr_reg.attr,
	&dev_attr_fast_calibration_x.attr,
	&dev_attr_fast_calibration_y.attr,
	&dev_attr_fast_calibration_z.attr,
	&dev_attr_eeprom.attr,	
	NULL
};

static struct attribute_group bma250_attribute_group = {
	.attrs = bma250_attributes
};



static int bma250_detect(struct i2c_client *client,
		struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	strlcpy(info->type, SENSOR_NAME, I2C_NAME_SIZE);

	return 0;
}

struct regulator *temp;
static int bma250_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	int tempvalue;
	struct bma250_data *data;
	struct input_dev *dev;

	data = kzalloc(sizeof(struct bma250_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	/*For g sensor*/
	data->vdd_regulator = regulator_get(NULL, "g-sensor-pwr");
	if (IS_ERR(data->vdd_regulator)) {
		printk("%s: regulator_get error\n",__func__);
		//goto err0;
	}
	err = regulator_set_voltage(data->vdd_regulator, 1800000,1800000);
	if (err) {
		printk("%s: regulator_set 1.8V error\n",__func__);
		//goto err0;
	}
	regulator_enable(data->vdd_regulator);
	temp=data->vdd_regulator;

	//Apply settings to get VAUX2 belonging to APP groug, set 0x1 to VAUX2_CFG_GRP
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01,0x88);
	udelay(5);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		goto exit;
	}
	/* read chip id */
	tempvalue = 0;
	tempvalue = i2c_smbus_read_word_data(client, BMA250_CHIP_ID_REG);

	if ((tempvalue&0x00FF) == BMA250_CHIP_ID) {
		printk(KERN_INFO "Bosch Sensortec Device detected!\n" \
				"BMA250 registered I2C driver!\n");
	} else{
		printk(KERN_INFO "Bosch Sensortec Device not found, \
				i2c error %d \n", tempvalue);
		err = -ENODEV;
		goto kfree_exit;
	}
	i2c_set_clientdata(client, data);
	data->bma250_client = client;
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	bma250_set_bandwidth(client, BMA250_BW_SET);
	bma250_set_range(client, BMA250_RANGE_SET);
	data->report_threshold = BMA250_REPORT_SET;

	INIT_DELAYED_WORK(&data->work, bma250_work_func);
	atomic_set(&data->delay, BMA250_MAX_DELAY);
	atomic_set(&data->enable, 0);
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;
	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, LOW_G_INTERRUPT);
	input_set_capability(dev, EV_REL, HIGH_G_INTERRUPT);
	input_set_capability(dev, EV_REL, SLOP_INTERRUPT);
	input_set_capability(dev, EV_REL, DOUBLE_TAP_INTERRUPT);
	input_set_capability(dev, EV_REL, SINGLE_TAP_INTERRUPT);
	input_set_capability(dev, EV_ABS, ORIENT_INTERRUPT);
	input_set_capability(dev, EV_ABS, FLAT_INTERRUPT);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto kfree_exit;
	}

	data->input = dev;

	err = sysfs_create_group(&data->input->dev.kobj,
						 &bma250_attribute_group);
	if (err < 0)
		goto error_sysfs;

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bma250_early_suspend;
	data->early_suspend.resume = bma250_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
	
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	bma250_set_enable(&client->dev, 1);	
	return 0;

error_sysfs:
	input_unregister_device(data->input);

kfree_exit:
	kfree(data);
exit:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma250_early_suspend(struct early_suspend *h)
{
	struct bma250_data *data =
		container_of(h, struct bma250_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable)==1) {
		bma250_set_mode(data->bma250_client, BMA250_MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);
}


static void bma250_late_resume(struct early_suspend *h)
{
	struct bma250_data *data =
		container_of(h, struct bma250_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable)==1) {
		bma250_set_mode(data->bma250_client, BMA250_MODE_NORMAL);
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
	}
	
	mutex_unlock(&data->enable_mutex);
}
#endif

static void bma250_shutdown(struct i2c_client *client)
{
	struct bma250_data *data = i2c_get_clientdata(client);
	printk(KERN_DEBUG "%s\n", __func__);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable)==1) {
		bma250_set_mode(data->bma250_client, BMA250_MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);
}

static int __devexit bma250_remove(struct i2c_client *client)
{
	struct bma250_data *data = i2c_get_clientdata(client);

	bma250_set_enable(&client->dev, 0);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bma250_attribute_group);
	input_unregister_device(data->input);
	kfree(data);

	return 0;
}


static const struct i2c_device_id bma250_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma250_id);

static struct i2c_driver bma250_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SENSOR_NAME,
	},
	.id_table	= bma250_id,
	.probe		= bma250_probe,
	.remove		= __devexit_p(bma250_remove),
	.detect		= bma250_detect,
	.shutdown	= bma250_shutdown,
};

static int __init BMA250_init(void)
{
	return i2c_add_driver(&bma250_driver);
}

static void __exit BMA250_exit(void)
{
	i2c_del_driver(&bma250_driver);
}

MODULE_DESCRIPTION("BMA250 driver");
MODULE_LICENSE("GPL");

module_init(BMA250_init);
module_exit(BMA250_exit);

