#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/input/ili210x.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#endif

#define MAX_TOUCHES		2
#define DEFAULT_POLL_PERIOD	20

/* Touchscreen commands */
#define REG_TOUCHDATA		0x10
#define REG_SUB_DATA		0x11
#define REG_PANEL_INFO		0x20
#define REG_FIRMWARE_VERSION	0x40
#define REG_PROTOCOL_VERSION	0x42
#define REG_CALIBRATE		0xcc
#define REG_ERASE_BACKGROUND	0xce

/* Definitions */
#define ILITEK_TS_RESET		104

MODULE_AUTHOR("Olivier Sobrie <olivier@xxxxxxxxx>");
MODULE_DESCRIPTION("ILI210X I2C Touchscreen Driver");
MODULE_LICENSE("GPL");

struct finger {
	u8 x_low;
	u8 x_high;
	u8 y_low;
	u8 y_high;
} __packed;

struct touchdata {
	u8 status;
	struct finger finger[MAX_TOUCHES];
} __packed;

struct panel_info {
	struct finger finger_max;
	u8 xchannel_num;
	u8 ychannel_num;
} __packed;

struct firmware_version {
	u8 id;
	u8 major;
	u8 minor;
} __packed;

struct ili210x {
	struct i2c_client *client;
	struct input_dev *input;
	bool (*get_pendown_state)(void);
	unsigned int poll_period;
	struct delayed_work dwork;

        // check whether i2c driver is registered success
        int valid_i2c_register;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

/* Global variables */
struct regulator *p_regulator;
static struct ili210x i2c;


static int ili210x_read_reg(struct i2c_client *client, u8 reg, void *buf,
			    size_t len)
{
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		dev_err(&client->dev, "i2c transfer failed\n");
		return -EIO;
	}

	return 0;
}

static void ili210x_report_events(struct input_dev *input,
				  const struct touchdata *touchdata)
{
	int i;
	bool touch;
	unsigned int x, y;
	const struct finger *finger;

	for (i = 0; i < MAX_TOUCHES; i++) {
		input_mt_slot(input, i);

		finger = &touchdata->finger[i];

		touch = touchdata->status & (1 << i);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, touch);
		if (touch) {
			x = finger->x_low | (finger->x_high << 8);
			y = finger->y_low | (finger->y_high << 8);

			input_report_abs(input, ABS_MT_POSITION_X, x);
			input_report_abs(input, ABS_MT_POSITION_Y, y);
		}
	}

	input_mt_report_pointer_emulation(input, false);
	input_sync(input);
}

static bool get_pendown_state(const struct ili210x *priv)
{
	bool state = false;

	if (priv->get_pendown_state)
		state = priv->get_pendown_state();

	return state;
}

static void ili210x_work(struct work_struct *work)
{
	struct ili210x *priv = container_of(work, struct ili210x,
					    dwork.work);
	struct input_dev *input = priv->input;
	struct i2c_client *client = priv->client;
	struct device *dev = &client->dev;
	struct touchdata touchdata;
	int rc;

	rc = ili210x_read_reg(client, REG_TOUCHDATA, &touchdata,
			      sizeof(touchdata));
	if (rc < 0) {
		dev_err(dev, "Unable to get touchdata, err = %d\n",
			rc);
		return;
	}

	ili210x_report_events(input, &touchdata);

	if ((touchdata.status & 0xf3) || get_pendown_state(priv))
		schedule_delayed_work(&priv->dwork,
				      msecs_to_jiffies(priv->poll_period));
}

static irqreturn_t ili210x_irq(int irq, void *irq_data)
{
	struct ili210x *priv = irq_data;

	schedule_delayed_work(&priv->dwork, 0);

	return IRQ_HANDLED;
}

static ssize_t ili210x_calibrate(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ili210x *priv = dev_get_drvdata(dev);
	unsigned long calibrate;
	int rc;
	u8 cmd = REG_CALIBRATE;

	if (kstrtoul(buf, 10, &calibrate))
		return -EINVAL;

	if (calibrate > 1)
		return -EINVAL;

	if (calibrate) {
		rc = i2c_master_send(priv->client, &cmd, sizeof(cmd));
		if (rc != sizeof(cmd))
			return -EIO;
	}

	return count;
}
static DEVICE_ATTR(calibrate, 0644, NULL, ili210x_calibrate);

static struct attribute *ili210x_attributes[] = {
	&dev_attr_calibrate.attr,
	NULL,
};

static const struct attribute_group ili210x_attr_group = {
	.attrs = ili210x_attributes,
};

static int __devinit ili210x_i2c_probe(struct i2c_client *client,
				       const struct i2c_device_id *id)
{
	struct ili210x *priv;
	struct device *dev = &client->dev;
	struct ili210x_platform_data *pdata = dev->platform_data;
	struct panel_info panel;
	struct firmware_version firmware;
	int xmax, ymax;
	int rc;

	pr_info("Probing for ILI210X I2C Touschreen driver\n");

	if (!pdata) {
		dev_err(dev, "No platform data!\n");
		return -ENODEV;
	}

	if (client->irq <= 0) {
		dev_err(dev, "No IRQ!\n");
		return -ENODEV;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Failed to allocate driver data!\n");
		rc = -ENOMEM;
		goto fail;
	}

	dev_set_drvdata(dev, priv);

	priv->input = input_allocate_device();
	if (!priv->input) {
		dev_err(dev, "Failed to allocate input device\n");
		rc = -ENOMEM;
		goto fail;
	}

	/* Get firmware version */
	rc = ili210x_read_reg(client, REG_FIRMWARE_VERSION, &firmware,
			      sizeof(firmware));
	if (rc < 0) {
		dev_err(dev, "Failed to get firmware version, err: %d\n",
			rc);
		goto fail_probe;
	}
	pr_info("Found Ilitek firmware version id=%d, major=%d, minor=%d", firmware.id, firmware.major, firmware.minor);

	/* get panel info */
	rc = ili210x_read_reg(client, REG_PANEL_INFO, &panel,
			      sizeof(panel));
	if (rc < 0) {
		dev_err(dev, "Failed to get panel informations, err: %d\n",
			rc);
		goto fail_probe;
	}

	xmax = panel.finger_max.x_low | (panel.finger_max.x_high << 8);
	ymax = panel.finger_max.y_low | (panel.finger_max.y_high << 8);

	/* Setup input device */
	__set_bit(EV_SYN, priv->input->evbit);
	__set_bit(EV_KEY, priv->input->evbit);
	__set_bit(EV_ABS, priv->input->evbit);
	__set_bit(BTN_TOUCH, priv->input->keybit);

	/* Single touch */
	input_set_abs_params(priv->input, ABS_X, 0, xmax, 0, 0);
	input_set_abs_params(priv->input, ABS_Y, 0, ymax, 0, 0);

	/* Multi touch */
	input_mt_init_slots(priv->input, MAX_TOUCHES);
	input_set_abs_params(priv->input, ABS_MT_POSITION_X, 0, xmax, 0, 0);
	input_set_abs_params(priv->input, ABS_MT_POSITION_Y, 0, ymax, 0, 0);

	priv->input->name = "ILI210x Touchscreen";
	priv->input->id.bustype = BUS_I2C;
	priv->input->dev.parent = dev;

	input_set_drvdata(priv->input, priv);

	priv->client = client;
	priv->get_pendown_state = pdata->get_pendown_state;
	priv->poll_period = pdata->poll_period ? : DEFAULT_POLL_PERIOD;
	INIT_DELAYED_WORK(&priv->dwork, ili210x_work);

	rc = request_irq(client->irq, ili210x_irq, pdata->irq_flags,
			 client->name, priv);
	if (rc) {
		dev_err(dev, "Unable to request touchscreen IRQ, err: %d\n",
			rc);
		goto fail_probe;
	}

	rc = sysfs_create_group(&dev->kobj, &ili210x_attr_group);
	if (rc) {
		dev_err(dev, "Unable to create sysfs attributes, err: %d\n",
			rc);
		goto fail_sysfs;
	}

	rc = input_register_device(priv->input);
	if (rc) {
		dev_err(dev, "Cannot regiser input device, err: %d\n", rc);
		goto fail_input;
	}

	device_init_wakeup(&client->dev, 1);

	dev_dbg(dev,
		"ILI210x initialized (IRQ: %d), firmware version %d.%d.%d",
		client->irq, firmware.id, firmware.major, firmware.minor);

	i2c.client = client;
	i2c.client = client;

	return 0;

fail_input:
	sysfs_remove_group(&dev->kobj, &ili210x_attr_group);
fail_sysfs:
	free_irq(client->irq, priv);
fail_probe:
	input_free_device(priv->input);
fail:
	kfree(priv);
	return rc;
}

static int __devexit ili210x_i2c_remove(struct i2c_client *client)
{
	struct ili210x *priv = dev_get_drvdata(&client->dev);
	struct device *dev = &priv->client->dev;

	pr_info("%s, ENTER\n", __func__);
	sysfs_remove_group(&dev->kobj, &ili210x_attr_group);
	free_irq(priv->client->irq, priv);
	cancel_delayed_work_sync(&priv->dwork);
	input_unregister_device(priv->input);
	kfree(priv);
	pr_info("%s, EXIT\n", __func__);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ili210x_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	pr_info("%s, ENTER\n", __func__);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(client->irq);

#if 0
	i2c.is_suspend = 1;
	if(i2c.valid_irq_request != 0) {
		disable_irq(client->irq);
	}
	else {
		i2c.stop_polling = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread polling\n", __func__);
  	}
//	flush_workqueue(i2c.irq_work_queue);
	msleep(1);
//	del_timer_sync(&i2c.timer);
	//mdelay(10);
#endif

	gpio_direction_output(ILITEK_TS_RESET ,0);
	if(regulator_disable(p_regulator))
		printk("%s regulator disable fail!\n",__func__);
	//gpio_171 and gpio_172 input pin for diagnostic to pull low
	omap_writew(omap_readw(0x4a10017C) | 0x010B, 0x4a10017C);
	omap_writew(omap_readw(0x4a10017C) | 0x010B, 0x4a10017E);
	//Disable I2C2 internal pull high
	omap_writel(omap_readl(0x4a100604) | 0x00100010,0x4a100604);

	pr_info("%s, EXIT\n", __func__);
	return 0;
}

static int ili210x_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	pr_info("%s, ENTER\n", __func__);
	if (device_may_wakeup(&client->dev))
		disable_irq_wake(client->irq);

//	i2c.is_suspend = 0;
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

#if 0
	if (i2c.valid_irq_request != 0) {
		enable_irq(client->irq);
	}
	else {
		i2c.stop_polling = 0;
		printk(ILITEK_DEBUG_LEVEL "%s, start i2c thread polling\n", __func__);
	}
#endif
	pr_info("%s, EXIT\n", __func__);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ili210x_i2c_pm, ili210x_i2c_suspend,
			 ili210x_i2c_resume);

static const struct i2c_device_id ili210x_i2c_id[] = {
	{ "ili210x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ili210x_i2c_id);

static struct i2c_driver ili210x_ts_driver = {
	.driver = {
		.name = "ili210x_i2c",
		.owner = THIS_MODULE,
		.pm = &ili210x_i2c_pm,
	},
	.id_table = ili210x_i2c_id,
	.probe = ili210x_i2c_probe,
	.remove = __devexit_p(ili210x_i2c_remove),
};
// module_i2c_driver(ili210x_ts_driver);

static int __init ili210x_ts_init(void) {
	int ret = 0;
	pr_info("%s, ENTER\n", __func__);
	if(gpio_request(ILITEK_TS_RESET, "ilitek_ts_reset")) {
		printk("Unable to request ilitek_ts_reset pin.\n");
		return -1;
	}
	gpio_direction_output(ILITEK_TS_RESET ,0);

	p_regulator = regulator_get(NULL, "vaux3");
	if(IS_ERR(p_regulator)) {
		printk("%s regulator_get error\n", __func__);
		return -1;
	}
	ret = regulator_set_voltage(p_regulator, 3000000, 3000000);
	if(ret) {
		printk("ilitek_init regulator_set 3.0V error\n");
		return -1;
	}
	regulator_enable(p_regulator);

	msleep(1);
	gpio_direction_output(ILITEK_TS_RESET, 1);
	msleep(15);

    	memset(&i2c, 0, sizeof(struct ili210x));

	ret = i2c_add_driver(&ili210x_ts_driver);
	if (ret == 0) {
		i2c.valid_i2c_register = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, add i2c device, success\n", __func__);
		if (i2c.client == NULL){
			printk(ILITEK_ERROR_LEVEL "%s, no i2c board information\n", __func__);
			return -1;
		}
#if 0
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
		ilitek_set_input_param(i2c.input_dev, i2c.max_tp, i2c.max_y, i2c.max_x);
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

		if(false){
			i2c.irq_work_queue = create_singlethread_workqueue("ilitek_i2c_irq_queue");
			if(i2c.irq_work_queue){
				INIT_WORK(&i2c.irq_work, ilitek_i2c_irq_work_queue_func);
		        init_timer(&i2c.timer);
		        i2c.timer.data = (unsigned long)&i2c;
		        i2c.timer.function = ilitek_i2c_timer;
				if(request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_LOW, "ilitek_i2c_irq", &i2c)){
					printk(ILITEK_ERROR_LEVEL "%s, request irq, error\n", __func__);
				}
				else{
					i2c.valid_irq_request = 1;
					printk(ILITEK_ERROR_LEVEL "%s, request irq, success\n", __func__);
				}
			}
		}else{
			i2c.stop_polling = 0;
			i2c.thread = kthread_create(ilitek_i2c_polling_thread, NULL, "ilitek_i2c_thread");
			if(i2c.thread == (struct task_struct*)ERR_PTR){
				i2c.thread = NULL;
				printk(ILITEK_ERROR_LEVEL "%s, kthread create, error\n", __func__);
			}else{
				wake_up_process(i2c.thread);
			}
		}

#endif
	}
	else{
		printk(ILITEK_ERROR_LEVEL "%s, add i2c device, error\n", __func__);
		return ret;
	}
	pr_info("%s, EXIT\n", __func__);
	return ret;
}

static void __exit ili210x_ts_exit(void) {
	pr_info("%s, ENTER\n", __func__);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&i2c.early_suspend);
#endif
#if 0
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
	else{
        	if(i2c.thread != NULL){
                	kthread_stop(i2c.thread);
                	printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread\n", __func__);
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
#endif
	pr_info("%s, EXIT\n", __func__);
}

/* set init and exit function for this module */
module_init(ili210x_ts_init);
module_exit(ili210x_ts_exit);

