/*
	Copyright (c) 2010 by ilitek Technology.
	All rights reserved.

	ilitek I2C touch screen driver for Android platform

	Author:	 Steward Fu
	Version: 1
	History:
		2010/10/26 Firstly released
		2010/10/28 Combine both i2c and hid function together
		2010/11/02 Support interrupt trigger for I2C interface
		2010/11/10 Rearrange code and add new IOCTL command
		2010/11/23 Support dynamic to change I2C address
		2010/12/21 Support resume and suspend functions
		2010/12/23 Fix synchronous problem when application and driver work at the same time
		2010/12/28 Add erasing background before calibrating touch panel
		2011/01/13 Rearrange code and add interrupt with polling method
		2011/01/14 Add retry mechanism
		2011/01/17 Support multi-point touch
		2011/01/21 Support early suspend function
		2011/02/14 Support key button function
		2011/02/18 Rearrange code
		2011/03/21 Fix counld not report first point
		2011/03/25 Support linux 2.36.x 

	Author:	 Hashcode
	Version: 2
	History:
		2012/06/06 Updated for 3.0.x kernel
*/
#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <plat/io.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#endif

// definitions
#define ILITEK_I2C_RETRY_COUNT			3
#define ILITEK_I2C_DRIVER_NAME			"ilitek_i2c"
#define ILITEK_FILE_DRIVER_NAME			"ilitek_file"
#define ILITEK_DEBUG_LEVEL			KERN_INFO
#define ILITEK_ERROR_LEVEL			KERN_ALERT
#define ILITEK_TS_RESET                         104

#define ILITEK_DEFAULT_WIDTH			3
#define ILITEK_DEFAULT_PRESSURE			15
#define ILITEK_DEFAULT_POLL_DELAY		2

// i2c command for ilitek touch screen
#define ILITEK_TP_CMD_READ_DATA			0x10
#define ILITEK_TP_CMD_READ_SUB_DATA		0x11
#define ILITEK_TP_CMD_GET_RESOLUTION		0x20
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION	0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION	0x42
#define	ILITEK_TP_CMD_CALIBRATION		0xCC
#define ILITEK_TP_CMD_ERASE_BACKGROUND		0xCE

// define the application command
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, unsigned char*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, unsigned char*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, unsigned char*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, unsigned char*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int)
#define ILITEK_IOCTL_I2C_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 8, int)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int)
#define ILITEK_IOCTL_I2C_SET_ADDRESS            _IOWR(ILITEK_IOCTL_BASE, 10, int)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int)
#define ILITEK_IOCTL_GET_INTERFANCE             _IOWR(ILITEK_IOCTL_BASE, 14, int)//default setting is i2c interface
#define ILITEK_IOCTL_I2C_SWITCH_IRQ             _IOWR(ILITEK_IOCTL_BASE, 15, int)

// all implemented global functions must be defined in here 
// in order to know how many function we had implemented
static int ilitek_i2c_register_device(void);
static void ilitek_set_input_param(struct input_dev*, int, int, int);
static int ilitek_i2c_read_tp_info(void);
static int ilitek_init(void);
static void ilitek_exit(void);

// i2c functions
static int ilitek_i2c_transfer(struct i2c_client*, struct i2c_msg*, int);
static int ilitek_i2c_read(struct i2c_client*, uint8_t, uint8_t*, int);
static int ilitek_i2c_process_and_report(void);

static int ilitek_i2c_suspend(struct i2c_client*, pm_message_t);
static int ilitek_i2c_resume(struct i2c_client*);
static void ilitek_i2c_shutdown(struct i2c_client*);
static int ilitek_i2c_probe(struct i2c_client*, const struct i2c_device_id*);
static int ilitek_i2c_remove(struct i2c_client*);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_early_suspend(struct early_suspend *h);
static void ilitek_i2c_late_resume(struct early_suspend *h);
#endif
static irqreturn_t ilitek_i2c_isr(int, void*);
static void ilitek_i2c_irq_work_queue_func(struct work_struct*);

// file operation functions
static int ilitek_file_open(struct inode*, struct file*);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static int ilitek_file_open(struct inode*, struct file*);
static ssize_t ilitek_file_write(struct file*, const char*, size_t, loff_t*);
static ssize_t ilitek_file_read(struct file*, char*, size_t, loff_t*);
static int ilitek_file_close(struct inode*, struct file*);

static int touch_filter = 0;
static bool touch_move = false;

// declare i2c data member
struct i2c_data {
	// input device
        struct input_dev *input_dev;
        // i2c client
        struct i2c_client *client;
        // maximum x
        int max_x;
        // maximum y
        int max_y;
	// maximum touch point
	int max_tp;
	// maximum key button
	int max_btn;
        // the total number of x channel
        int x_ch;
        // the total number of y channel
        int y_ch;
        // check whether i2c driver is registered success
        int valid_i2c_register;
        // check whether input driver is registered success
        int valid_input_register;
	// check whether the i2c enter suspend or not
	int stop_polling;
	// read semaphore
	struct semaphore wr_sem;
	// protocol version
	int protocol_ver;
	// valid irq request
	int valid_irq_request;
	int is_suspend;
	// last touch
	int last_touch1_x;
	int last_touch1_y;
	int last_touch2_x;
	int last_touch2_y;
	// work queue for interrupt use only
	struct workqueue_struct *irq_work_queue;
	// work struct for work queue
	struct work_struct irq_work;
	struct timer_list timer;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

// device data
struct dev_data {
        // device number
        dev_t devno;
        // character device
        struct cdev cdev;
        // class device
        struct class *class;
};

// global variables
static struct i2c_data i2c;
static struct dev_data dev;
struct regulator *p_regulator;

// declare file operations
struct file_operations ilitek_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl = ilitek_file_ioctl,
#else
	.ioctl = ilitek_file_ioctl,
#endif
	.read = ilitek_file_read,
	.write = ilitek_file_write,
	.open = ilitek_file_open,
	.release = ilitek_file_close,
};

static int ilitek_file_open(struct inode *inode, struct file *filp) {
	return 0; 
}

static ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
	int ret;
	unsigned char buffer[128]={0};
        struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = count, .buf = buffer,}
	};
        
	// before sending data to touch device, we need to check whether the device is working or not
	if(i2c.valid_i2c_register == 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c device driver doesn't be registered\n", __func__);
		return -1;
	}

	// check the buffer size whether it exceeds the local buffer size or not
	if(count > 128){
		printk(ILITEK_ERROR_LEVEL "%s, buffer exceed 128 bytes\n", __func__);
		return -1;
	}

	// copy data from user space
	ret = copy_from_user(buffer, buf, count-1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed", __func__);
		return -1;
	}

	// parsing command
	if(strcmp(buffer, "calibrate") == 0){
		buffer[0] = ILITEK_TP_CMD_ERASE_BACKGROUND;
                msgs[0].len = 1;
                ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
                if(ret < 0){
                        printk(ILITEK_DEBUG_LEVEL "%s, i2c erase background, failed\n", __func__);
                }
                else{
                        printk(ILITEK_DEBUG_LEVEL "%s, i2c erase background, success\n", __func__);
                }

		buffer[0] = ILITEK_TP_CMD_CALIBRATION;
                msgs[0].len = 1;
                msleep(2000);
                ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
		if(ret < 0){
                        printk(ILITEK_DEBUG_LEVEL "%s, i2c calibration, failed\n", __func__);
                }
		else{
                	printk(ILITEK_DEBUG_LEVEL "%s, i2c calibration, success\n", __func__);
		}
		msleep(1000);
                return count;
	}
	return -1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
#endif
	static unsigned char buffer[64]={0};
	static int len=0;
	int ret;
	struct i2c_msg msgs[] = {
		{.addr = i2c.client->addr, .flags = 0, .len = len, .buf = buffer,}
        };

	// parsing ioctl command
	switch(cmd){
		case ILITEK_IOCTL_I2C_WRITE_DATA:
			ret = copy_from_user(buffer, (unsigned char*)arg, len);
			if(ret < 0){
						printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed\n", __func__);
						return -1;
				}
			ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, i2c write, failed\n", __func__);
				return -1;
			}
			break;
		case ILITEK_IOCTL_I2C_READ_DATA:
			msgs[0].flags = I2C_M_RD;
	
			ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, i2c read, failed\n", __func__);
				return -1;
			}
			ret = copy_to_user((unsigned char*)arg, buffer, len);
			
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
				return -1;
			}
			break;
		case ILITEK_IOCTL_I2C_WRITE_LENGTH:
		case ILITEK_IOCTL_I2C_READ_LENGTH:
			len = arg;
			break;
		case ILITEK_IOCTL_I2C_UPDATE_RESOLUTION:
		case ILITEK_IOCTL_I2C_SET_ADDRESS:
		case ILITEK_IOCTL_I2C_UPDATE:
			break;
		case ILITEK_IOCTL_START_READ_DATA:
			i2c.stop_polling = 0;
			break;
		case ILITEK_IOCTL_STOP_READ_DATA:
			i2c.stop_polling = 1;
			break;
		case ILITEK_IOCTL_I2C_SWITCH_IRQ:
			ret = copy_from_user(buffer, (unsigned char*)arg, 1);
			if (buffer[0]==0)
			{
				printk("%s, disabled irq\n",__func__);
				disable_irq(i2c.client->irq);
			}
			else
			{
				printk("%s, enabled irq\n",__func__);
				enable_irq(i2c.client->irq);
			}
			break;	
		default:
			return -1;
	}
    	return 0;
}

static ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	return 0;
}

static int ilitek_file_close(struct inode *inode, struct file *filp) {
        return 0;
}

static void ilitek_set_input_param(struct input_dev *input, int max_tp, int max_x, int max_y) {
	// __set_bit(EV_SYN, input->evbit);
	input->evbit[0] = BIT_MASK(EV_ABS);
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
        input_set_abs_params(input, ABS_MT_POSITION_X, 0, max_x + 1, 0, 0);
        input_set_abs_params(input, ABS_MT_POSITION_Y, 0, max_y + 1, 0, 0);
        input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, max_tp, 0, 0);
	input->name = ILITEK_I2C_DRIVER_NAME;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &(i2c.client)->dev;
}

static int ilitek_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs, int cnt) {
	int ret, count=ILITEK_I2C_RETRY_COUNT;
	while (count >= 0) {
		count-= 1;
		ret = down_interruptible(&i2c.wr_sem);
                ret = i2c_transfer(client->adapter, msgs, cnt);
                up(&i2c.wr_sem);
                if (ret < 0) {
                        msleep(500);
			continue;
                }
		break;
	}
	return ret;
}

static int ilitek_i2c_read(struct i2c_client *client, uint8_t cmd, uint8_t *data, int length) {
	int ret;
        struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = 1, .buf = &cmd,},
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
        };

        ret = ilitek_i2c_transfer(client, msgs, 2);
	if (ret < 0) {
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
	}
	return ret;
}

static int ilitek_i2c_process_and_report(void) {
	int i, ret, org_x, org_y;
	int x1 = 0;
	int y1 = 0;
	int x2 = 0;
	int y2 = 0;
	unsigned char buf[9]={0};
	int width = ILITEK_DEFAULT_WIDTH;
	int pressure = ILITEK_DEFAULT_PRESSURE;
	bool changed = false;

	// read i2c data from device
	ret = ilitek_i2c_read(i2c.client, ILITEK_TP_CMD_READ_DATA, buf, 9);
	if (ret < 0)
		return ret;

	// dual process
	if (!(i2c.protocol_ver & 0x200)) {
		// parse point

		unsigned char tp_id;
		tp_id = buf[0];
		i = 0;

		// release point
		if (buf[0] == 0) {
			// input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, false);
			if ((i2c.last_touch1_x != 0) || (i2c.last_touch1_y != 0) || (i2c.last_touch2_x != 0) || (i2c.last_touch2_y != 0)) {
				i2c.last_touch1_x = 0;
				i2c.last_touch1_y = 0;
				i2c.last_touch2_x = 0;
				i2c.last_touch2_y = 0;
				input_mt_sync(i2c.input_dev);
			}
			touch_move = false;
		}
		else {
			if (tp_id & 0x01) {
				i = 0;
				org_x = (int)buf[1 + (i * 4)] + ((int)buf[2 + (i * 4)] * 256);
				org_y = (int)buf[3 + (i * 4)] + ((int)buf[4 + (i * 4)] * 256);

#ifdef CONFIG_TOUCHSCREEN_ILITEK_SWAP_XY
				x1 = org_y;
				y1 = org_x;
#else
				x1 = org_x;
				y1 = org_y;
#endif
#ifdef CONFIG_TOUCHSCREEN_ILITEK_SWAP_XY
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_X
				y1 = (i2c.max_y - y1);
#endif
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_Y
				x1 = (i2c.max_x - x1);
#endif
#else
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_X
				x1 = (i2c.max_x - x1);
#endif
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_Y
				y1 = (i2c.max_y - y1);
#endif
#endif
				// Workaround for wiping from right to left when device is rotated
				if (x1 == 0) x1++;
				if (y1 == 0) y1++;

				if (touch_filter > 0 && !touch_move && i2c.last_touch1_x != 0 && i2c.last_touch1_y != 0) {
					if ((i2c.last_touch1_x + touch_filter < x1) ||
							(i2c.last_touch1_x - touch_filter > x1) ||
							(i2c.last_touch1_y + touch_filter < y1) ||
							(i2c.last_touch1_y - touch_filter > y1)) {
						touch_move = true;
						changed = true;
					}
				}
				else {
					if ((i2c.last_touch1_x != x1) || (i2c.last_touch1_y != y1))
						changed = true;
				}
			}

			if (tp_id & 0x02) {
				i = 1;
				org_x = (int)buf[1 + (i * 4)] + ((int)buf[2 + (i * 4)] * 256);
				org_y = (int)buf[3 + (i * 4)] + ((int)buf[4 + (i * 4)] * 256);

#ifdef CONFIG_TOUCHSCREEN_ILITEK_SWAP_XY
				x2 = org_y;
				y2 = org_x;
#else
				x2 = org_x;
				y2 = org_y;
#endif
#ifdef CONFIG_TOUCHSCREEN_ILITEK_SWAP_XY
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_X
				y2 = (i2c.max_y - y2);
#endif
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_Y
				x2 = (i2c.max_x - x2);
#endif
#else
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_X
				x2 = (i2c.max_x - x2);
#endif
#ifdef CONFIG_TOUCHSCREEN_ILITEK_FLIP_Y
				y2 = (i2c.max_y - y2);
#endif
#endif

				if ((i2c.last_touch2_x != x2) || (i2c.last_touch2_y != y2))
					changed = true;
			}

			
			if (changed) {
				if (tp_id & 0x01) {
					i2c.last_touch1_x = x1;
					i2c.last_touch1_y = y1;
					// input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, true);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, 0);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, width);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x1);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y1);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_PRESSURE, pressure);
					input_mt_sync(i2c.input_dev);
				}
				if (tp_id & 0x02) {
					i2c.last_touch2_x = x2;
					i2c.last_touch2_y = y2;
					// input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, true);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, 1);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, width);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x2);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y2);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_PRESSURE, pressure);
					input_mt_sync(i2c.input_dev);
				}
			}
		}
		input_sync(i2c.input_dev);
	}
        return 0;
}

static void ilitek_i2c_timer(unsigned long handle) {
	struct i2c_data *priv = (void *)handle;
	schedule_work(&priv->irq_work);
}

static void ilitek_i2c_irq_work_queue_func(struct work_struct *work) {
	struct i2c_data *priv = container_of(work, struct i2c_data, irq_work);

	if (i2c.is_suspend) {
		enable_irq(i2c.client->irq);
		return;
	}

	if (ilitek_i2c_process_and_report() < 0) {
		msleep(3000);
		printk(ILITEK_ERROR_LEVEL "%s, process error\n", __func__);
	}
	if (gpio_get_value(35) == 0 && i2c.is_suspend == 0)
		mod_timer(&priv->timer, jiffies + msecs_to_jiffies(ILITEK_DEFAULT_POLL_DELAY));
	else
		enable_irq(i2c.client->irq);
}

static irqreturn_t ilitek_i2c_isr(int irq, void *dev_id) {
	if(i2c.is_suspend)
		return IRQ_HANDLED;

	disable_irq_nosync(i2c.client->irq);
	queue_work(i2c.irq_work_queue, &i2c.irq_work);
	return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_early_suspend(struct early_suspend *h)
{
	ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
	printk("%s\n", __func__);
}

static void ilitek_i2c_late_resume(struct early_suspend *h)
{
	ilitek_i2c_resume(i2c.client);
	printk("%s\n", __func__);
}
#endif

#ifdef CONFIG_PM
static int ilitek_i2c_suspend(struct i2c_client *client, pm_message_t mesg) {
	i2c.is_suspend = 1;
	if(i2c.valid_irq_request != 0){
		disable_irq(i2c.client->irq);
	}
	else {
		i2c.stop_polling = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread polling\n", __func__);
  	}
	flush_workqueue(i2c.irq_work_queue);
	msleep(1);
	del_timer_sync(&i2c.timer);
	//mdelay(10);

	gpio_direction_output(ILITEK_TS_RESET ,0);
	if (regulator_disable(p_regulator))
		printk("%s regulator disable fail!\n",__func__);
	//gpio_171 and gpio_172 input pin for diagnostic to pull low
	omap_writew(omap_readw(0x4a10017C) | 0x010B, 0x4a10017C);
	omap_writew(omap_readw(0x4a10017C) | 0x010B, 0x4a10017E);
	//Disable I2C2 internal pull high
	omap_writel(omap_readl(0x4a100604) | 0x00100010,0x4a100604);

	return 0;
}

static int ilitek_i2c_resume(struct i2c_client *client) {
	int retry = 1;
	struct irq_desc *desc = irq_to_desc(i2c.client->irq);

	i2c.is_suspend = 0;

	//Enable I2C2 internal pull high
	omap_writel(omap_readl(0x4a100604) & 0xFFEFFFEF,0x4a100604);
	if(regulator_enable(p_regulator))
		printk("%s regulator enable fail!\n",__func__);
	msleep(1);
	gpio_direction_output(ILITEK_TS_RESET ,1);
	msleep(15);
	//gpio_171 and gpio_172 input pin for diagnostic to pull high
	omap_writew(omap_readw(0x4a10017C) | 0x011B, 0x4a10017C);
	omap_writew(omap_readw(0x4a10017C) | 0x011B, 0x4a10017E);

	if (i2c.valid_irq_request != 0) {
		enable_irq(i2c.client->irq);
		while(desc->depth != 0 && retry <= 3) {
			printk("%s re-enable irq! retry = %d\n", __func__, retry);
			enable_irq(i2c.client->irq);
			retry++;
		}
	}
	else {
		i2c.stop_polling = 0;
		printk(ILITEK_DEBUG_LEVEL "%s, start i2c thread polling\n", __func__);
	}

	return 0;
}
#endif

static void ilitek_i2c_shutdown(struct i2c_client *client) {
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	i2c.stop_polling = 1;
}

static int ilitek_i2c_read_info(struct i2c_client *client, uint8_t cmd, uint8_t *data, int length);

static ssize_t ilitek_version_show(struct device *dev, struct device_attribute *attr, char *buf) {
	u8 ver[4];

	if (ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_FIRMWARE_VERSION, ver, 4) < 0) {
		return sprintf(buf, "-1\n");
	} else {
		return sprintf(buf, "%d.%d.%d.%d\n", ver[0], ver[1], ver[2], ver[3]);
	}
}

static ssize_t touch_filter_read(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf,"%d\n", touch_filter);
}

static ssize_t touch_filter_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d\n", &touch_filter);
	return size;
}

static DEVICE_ATTR(version, S_IRUGO, ilitek_version_show, NULL);
static DEVICE_ATTR(touch_filter, S_IRUGO | S_IWUGO, touch_filter_read, touch_filter_write);

static struct attribute *ilitek_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_touch_filter.attr,
	NULL
};

static struct attribute_group ilitek_attrs_group = {
	.attrs = ilitek_attrs,
};

static int ilitek_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	if (sysfs_create_group(&client->dev.kobj, &ilitek_attrs_group)) {
		printk(ILITEK_ERROR_LEVEL "%s: Unable to create sysfs group\n", __FUNCTION__);
		return -EINVAL;
	}
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
                printk(ILITEK_ERROR_LEVEL "%s, I2C_FUNC_I2C not support\n", __func__);
                return -1;
        }
	i2c.client = client;
        printk(ILITEK_DEBUG_LEVEL "%s, i2c new style format\n", __func__);
	printk("%s, IRQ: 0x%X\n", __func__, client->irq);
	return 0;
}

static int ilitek_i2c_remove(struct i2c_client *client) {
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	i2c.stop_polling = 1;
	return 0;
}

/*
description
	read data from i2c device with delay between cmd & return data
parameter
	client
		i2c client data
	addr
		i2c address
	data
		data for transmission
	length
		data length
return
	status
*/
static int ilitek_i2c_read_info(struct i2c_client *client, uint8_t cmd, uint8_t *data, int length) {
	int ret;
	struct i2c_msg msgs_cmd[] = {
		{ .addr = client->addr, .flags = 0, .len = 1, .buf = &cmd,},
	};
	
	struct i2c_msg msgs_ret[] = {
		{ .addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,},
	};

	ret = ilitek_i2c_transfer(client, msgs_cmd, 1);
	if (ret < 0)
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
	
	msleep(10);
	ret = ilitek_i2c_transfer(client, msgs_ret, 1);
	if (ret < 0)
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);		
	return ret;
}

/*
description
	read touch information
parameters
	none
return
	status
*/
static int ilitek_i2c_read_tp_info(void) {
	int res_len;
	unsigned char buf[32]={0};

	// read firmware version
        if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_FIRMWARE_VERSION, buf, 4) < 0){
		return -1;
	}
	printk(ILITEK_DEBUG_LEVEL "%s, firmware version %d.%d.%d.%d\n", __func__, buf[0], buf[1], buf[2], buf[3]);

	// read protocol version
        res_len = 6;
        if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_PROTOCOL_VERSION, buf, 2) < 0){
		return -1;
	}	
        i2c.protocol_ver = (((int)buf[0]) << 8) + buf[1];
        printk(ILITEK_DEBUG_LEVEL "%s, protocol version: %d.%d\n", __func__, buf[0], buf[1]);
        if(i2c.protocol_ver >= 0x200){
               	res_len = 8;
        }

        // read touch resolution
	i2c.max_tp = 2;
        if(ilitek_i2c_read_info(i2c.client, ILITEK_TP_CMD_GET_RESOLUTION, buf, res_len) < 0){
		return -1;
	}
        if(i2c.protocol_ver >= 0x200){
                // maximum touch point
                i2c.max_tp = buf[6];
                // maximum button number
                i2c.max_btn = buf[7];
        }
	// calculate the resolution for x and y direction
#ifdef CONFIG_TOUCHSCREEN_ILITEK_SWAP_XY
        i2c.max_y = buf[0];
        i2c.max_y+= ((int)buf[1]) * 256;
        i2c.max_x = buf[2];
        i2c.max_x+= ((int)buf[3]) * 256;
        i2c.y_ch = buf[4];
        i2c.x_ch = buf[5];
#else
        i2c.max_x = buf[0];
        i2c.max_x+= ((int)buf[1]) * 256;
        i2c.max_y = buf[2];
        i2c.max_y+= ((int)buf[3]) * 256;
        i2c.x_ch = buf[4];
        i2c.y_ch = buf[5];
#endif
        printk(ILITEK_DEBUG_LEVEL "%s, max_x: %d, max_y: %d, ch_x: %d, ch_y: %d\n", 
		__func__, i2c.max_x, i2c.max_y, i2c.x_ch, i2c.y_ch);
        printk(ILITEK_DEBUG_LEVEL "%s, max_tp: %d, max_btn: %d\n", __func__, i2c.max_tp, i2c.max_btn);
	return 0;
}

// i2c id table
static const struct i2c_device_id ilitek_i2c_id[] ={
	{ ILITEK_I2C_DRIVER_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ilitek_i2c_id);

// declare i2c function table
static struct i2c_driver ilitek_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name   = ILITEK_I2C_DRIVER_NAME
	},
	.class		= I2C_CLASS_HWMON,
	.id_table       = ilitek_i2c_id,
#ifndef CONFIG_PM
	.resume         = ilitek_i2c_resume,
	.suspend        = ilitek_i2c_suspend,
#endif
	.shutdown       = ilitek_i2c_shutdown,
	.probe          = ilitek_i2c_probe,
	.remove         = __devexit_p(ilitek_i2c_remove),
};

/*
description
	register i2c device and its input device
parameters
	none
return
	status
*/
static int ilitek_i2c_register_device(void) {
	int ret = i2c_add_driver(&ilitek_i2c_driver);
	if (ret == 0) {
		i2c.valid_i2c_register = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, add i2c device, success\n", __func__);
		if(i2c.client == NULL){
			printk(ILITEK_ERROR_LEVEL "%s, no i2c board information\n", __func__);
			return -1;
		}
		printk(ILITEK_DEBUG_LEVEL "%s, client.addr: 0x%X\n", __func__, (unsigned int)i2c.client->addr);
		printk(ILITEK_DEBUG_LEVEL "%s, client.adapter: 0x%X\n", __func__, (unsigned int)i2c.client->adapter);
		printk(ILITEK_DEBUG_LEVEL "%s, client.driver: 0x%X\n", __func__, (unsigned int)i2c.client->driver);
		if((i2c.client->addr == 0) || (i2c.client->adapter == 0) || (i2c.client->driver == 0)){
			printk(ILITEK_ERROR_LEVEL "%s, invalid register\n", __func__);
			return ret;
		}
		
		// read touch parameter
		ilitek_i2c_read_tp_info();

		// register input device
		i2c.input_dev = input_allocate_device();
		if(i2c.input_dev == NULL){
			printk(ILITEK_ERROR_LEVEL "%s, allocate input device, error\n", __func__);
			return -1;
		}
		ilitek_set_input_param(i2c.input_dev, i2c.max_tp, i2c.max_x, i2c.max_y);
        	ret = input_register_device(i2c.input_dev);
        	if(ret){
               		printk(ILITEK_ERROR_LEVEL "%s, register input device, error\n", __func__);
                	return ret;
        	}
               	printk(ILITEK_ERROR_LEVEL "%s, register input device, success\n", __func__);
		i2c.valid_input_register = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
		i2c.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		i2c.early_suspend.suspend = ilitek_i2c_early_suspend;
		i2c.early_suspend.resume = ilitek_i2c_late_resume;
		register_early_suspend(&i2c.early_suspend);
#endif

		if(i2c.client->irq != 0 ){
			i2c.irq_work_queue = create_singlethread_workqueue("ilitek_i2c_irq_queue");
			if(i2c.irq_work_queue) {
				INIT_WORK(&i2c.irq_work, ilitek_i2c_irq_work_queue_func);
				init_timer(&i2c.timer);
				i2c.timer.data = (unsigned long)&i2c;
				i2c.timer.function = ilitek_i2c_timer;
				if (request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_LOW, "ilitek_i2c_irq", &i2c)) {
					printk(ILITEK_ERROR_LEVEL "%s, request irq, error\n", __func__);
				}
				else {
					i2c.valid_irq_request = 1;
					printk(ILITEK_ERROR_LEVEL "%s, request irq, success\n", __func__);
				}
			}
		}
	}
	else {
		printk(ILITEK_ERROR_LEVEL "%s, add i2c device, error\n", __func__);
		return ret;
	}
	return 0;
}

static int ilitek_init(void) {
	int ret = 0;

	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	
	if (gpio_request(ILITEK_TS_RESET, "ilitek_ts_reset")) {
		printk("Unable to request ilitek_ts_reset pin.\n");
		return -1;
	}
	gpio_direction_output(ILITEK_TS_RESET ,0);

	p_regulator = regulator_get(NULL, "vaux3");
	if (IS_ERR(p_regulator)) {
		printk("%s regulator_get error\n", __func__);
		return -1;
	}
	ret = regulator_set_voltage(p_regulator, 3000000, 3000000);
	if (ret) {
		printk("ilitek_init regulator_set 3.0V error\n");
		return -1;
	}
	regulator_enable(p_regulator);

	msleep(1);
	gpio_direction_output(ILITEK_TS_RESET ,1);
	msleep(15);

	// initialize global variable
	memset(&dev, 0, sizeof(struct dev_data));
	memset(&i2c, 0, sizeof(struct i2c_data));

	// initialize mutex object
	sema_init(&i2c.wr_sem, 1);

	i2c.wr_sem.count = 1;
	
	// register i2c device
	ret = ilitek_i2c_register_device();
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, register i2c device, error\n", __func__);
		return ret;
	}

	// allocate character device driver buffer
	ret = alloc_chrdev_region(&dev.devno, 0, 1, ILITEK_FILE_DRIVER_NAME);
    	if (ret) {
        	printk(ILITEK_ERROR_LEVEL "%s, can't allocate chrdev\n", __func__);
		return ret;
	}
    	printk(ILITEK_DEBUG_LEVEL "%s, register chrdev(%d, %d)\n", __func__, MAJOR(dev.devno), MINOR(dev.devno));
	
	// initialize character device driver
	cdev_init(&dev.cdev, &ilitek_fops);
	dev.cdev.owner = THIS_MODULE;
    	ret = cdev_add(&dev.cdev, dev.devno, 1);
    	if (ret < 0) {
        	printk(ILITEK_ERROR_LEVEL "%s, add character device error, ret %d\n", __func__, ret);
		return ret;
	}
	dev.class = class_create(THIS_MODULE, ILITEK_FILE_DRIVER_NAME);
	if (IS_ERR(dev.class)) {
        	printk(ILITEK_ERROR_LEVEL "%s, create class, error\n", __func__);
		return ret;
    	}
    	device_create(dev.class, NULL, dev.devno, NULL, "ilitek_ctrl");
	return 0;
}

/*
description
	driver exit function
parameters
	none
return
	nothing
*/
static void ilitek_exit(void) {
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&i2c.early_suspend);
#endif
	// delete i2c driver
	if(i2c.client->irq != 0){
        	if(i2c.valid_irq_request != 0){
                	free_irq(i2c.client->irq, &i2c);
                	printk(ILITEK_DEBUG_LEVEL "%s, free irq\n", __func__);
                	if(i2c.irq_work_queue){
                        	destroy_workqueue(i2c.irq_work_queue);
                        	printk(ILITEK_DEBUG_LEVEL "%s, destory work queue\n", __func__);
                	}
        	}
	}
        if(i2c.valid_i2c_register != 0){
                i2c_del_driver(&ilitek_i2c_driver);
                printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver\n", __func__);
        }
        if(i2c.valid_input_register != 0){
                input_unregister_device(i2c.input_dev);
                printk(ILITEK_DEBUG_LEVEL "%s, unregister i2c input device\n", __func__);
        }
        
	// delete character device driver
	cdev_del(&dev.cdev);
	unregister_chrdev_region(dev.devno, 1);
	device_destroy(dev.class, dev.devno);
	class_destroy(dev.class);
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
}

/* set init and exit function for this module */
module_init(ilitek_init);
module_exit(ilitek_exit);

MODULE_AUTHOR("Steward_Fu");
MODULE_DESCRIPTION("ILITEK I2C touch driver for Android platform");
MODULE_LICENSE("GPL");
