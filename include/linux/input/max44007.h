#ifndef __MAX44007_H_

#define MAX44007_NAME			"MAX44007"
#define MAX44007_I2C_ADDRESS0     	0x94
#define MAX44007_I2C_ADDRESS1     	0x96
#define MAX44007_INT_STATUS_ADDR  	0x00
#define MAX44007_INT_ENABLE_ADDR 	0x01
#define MAX44007_CONFIG_ADDR 		0x02
#define MAX44007_LUX_HIGH_ADDR		0x03
#define MAX44007_LUX_LOW_ADDR		0x04
#define MAX44007_THRESH_HIGH_ADDR	0x05
#define MAX44007_THRESH_LOW_ADDR	0x06
#define MAX44007_THRESH_TIM_ADDR	0x07
#define MAX44007_CLK_COARSE_ADDR	0x09
#define MAX44007_CLK_FINE_ADDR		0x0A
#define MAX44007_GREEN_TRIM_ADDR	0x0B
#define MAX44007_IR_TRIM_ADDR		0x0C
#define MAX44007_OTP_REG		0x0D
#define MAX44007_RETRY_DELAY      	10
#define MAX44007_MAX_RETRIES      	5

#include <linux/types.h>

struct MAX44007PlatformData
{
	// put things here
	int placeHolder;
};

/* Anvoi, 2011/09/07 for ftm porting { */
/* IOCTL */
#define ACTIVE_LS			100
#define READ_LS				101
/* Anvoi, 2011/09/07 for ftm porting } */

/**
 *  A structure that defines an "operating zone" for the light sensor with upper
 *  and lower thresholds. The MAX44007 has two threshold registers that can be
 *  programmed to trigger an interrupt when the light level has exceeded the
 *  values for a specified amount of time.
 *
*/
struct MAX44007ThreshZone {
  u8 upperThresh;
  u8 lowerThresh;
};

enum chip_id {
	MAX44007 = 0,
	MAX44009,
};

struct MAX44007CalData {
  u16 greenConstant;
  u16 irConstant;
};

/**
 *  This structure has register data for the MAX44007, with each 8-bit register
 *  described for the sake of clarity. Please consult the datasheet for detailed
 *  information on each register
 *
 *  The settings here are meant to be a reflection of the internal register
 *  settings of the MAX44007, as a hope that it will reduce the need for reading
 *  the device as much (to save power)
 */
struct MAX44007Data {
  struct i2c_client *client;          // represents the slave device
  struct input_dev *idev;             // an input device
  struct delayed_work work_queue;
  struct MAX44007ThreshZone threshZones;
  struct MAX44007PlatformData *pdata;
  atomic_t enabled;
  spinlock_t irqLock;
  int irqState;                       // device's IRQ state
  struct regulator *regulator;

  enum chip_id	als_id;
  // internal settings follow...
  u8 interrupt_status;
  u8 interrupt_en;	// interrupt enable
  u8 config;		// configuration register
  u8 lux_high;		// lux high byte register
  u8 lux_low;		// lux low byte register
  u8 thresh_high;	// upper threshold: high byte
  u8 thresh_low;	// lower threshold: low byte
  u8 thresh_tim;	// threshold timer
  u8 clk_coarse;	// coarse clock trim
  u8 clk_fine;	// fine clock trim
  u16 green_trim;	// green trim register
  u16 ir_trim;	// ir trim register
  u16 green_trim_backup;
  u16 ir_trim_backup;
};

int max44007_read_reg(struct MAX44007Data *device,u8 *buffer,int length);
int max44007_write_reg(struct MAX44007Data *device,u8 *buffer,int length);
int max44007_set_interrupt(struct MAX44007Data *device,bool enable);
int max44007_read_int_status(struct MAX44007Data *device);
int max44007_set_integration_time(struct MAX44007Data *device,u8 time);
int max44007_set_manual_mode(struct MAX44007Data *device,bool enable);
int max44007_set_continuous_mode(struct MAX44007Data *device,bool enable);
int max44007_set_current_div_ratio(struct MAX44007Data *device,bool enable);
int max44007_set_thresh_tim(struct MAX44007Data *device,u8 new_tim_val);
int max44007_set_threshold_zone(struct MAX44007Data *device,
                                struct MAX44007ThreshZone *new_zone);
int max44007_report_light_level(struct MAX44007Data *alsDevice,
                                    bool clearInterruptFlag);
void max44007_enable_IRQ(struct MAX44007Data *device,bool enable);

#define __MAX44007_H_
#endif

