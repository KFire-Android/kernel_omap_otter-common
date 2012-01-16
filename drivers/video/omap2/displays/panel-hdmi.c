/*
 * adi7526 hdmi transmitter driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/completion.h>
#include <linux/kthread.h>

#define DRIVER_VERSION			"1.1.0"
#define DRIVER_NAME			"adi7526"

//Interrupt can not be detected from IRQ pin by setting ADI registers.
//Use polling mode now.
#define OMAP4_ADI7526_IRQ		122
//#define IRQ

//#define AUDIO		//Define:HDMI mode with audio. Do not define:DVI mode without audio.

struct adi7526_device_info {
	struct device 		*dev;
	struct i2c_client	*client;
	struct workqueue_struct *irq_workqueue;
#ifdef IRQ
	struct work_struct irq_work;
#else
	struct completion thread_completion;
	struct mutex adi_lock;
	int thread_running;
#endif
};

static const char adi_init[][2]={
	{ 0x41, 0x40 },//Power down circuit
	{ 0x41, 0x00 },//Power up circuit
	{ 0x98, 0x03 },
	{ 0x9C, 0x38 },
	{ 0x9D, 0x61 },
	{ 0xA2, 0xA0 },
	{ 0xA3, 0xA0 },
	{ 0xDE, 0x82 },
	{ 0x15, 0x00 },//InputID:24bitRGB, 44.1 kHz
	{ 0x16, 0x00 },//RGB,falling edge, style1. Ouput format 4:4:4
	{ 0xD5, 0x00 },//black image disable, code shift disabled, normal refresh rate,
	{ 0xBA, 0x30 },//clock delay = 0
	{ 0xD0, 0x30 },//DDR negative edge CLK delay = 0
#ifdef AUDIO
	{ 0xAF, 0x16 },//Frame encryption, HDMI mode
#else
	// DVI mode
#endif
	{ 0x01, 0x00 },//CTS read from 0x4-0x6=0xC7A7=51111, N=0x001880=6272
	{ 0x02, 0x18 },
	{ 0x03, 0x80 },
	{ 0x0A, 0x41 },//MCLK ratio 256xfs
	{ 0x0C, 0x3C },//Use sampling frequency from I2S stream, I2S input enable
	{ 0x17, 0x02 },//Enable DE generator
	{ 0x35, 0x40 },//Hsync delay=b100 0000 11= 0x103 =260
	{ 0x36, 0xD9 },//Vsync delay=0x9
	{ 0x37, 0x08 },//DE generation active width=b1000 0000 000 =0x400=1024
	{ 0x38, 0x00 },
	{ 0x39, 0x25 },//DE generation active height=b10 0101 1000=0x258=600
	{ 0x3A, 0x80 },
	{ 0x96, 0x80 },//Enable HPD interrupt
	{ 0x00, 0x00 },//End
};

static struct adi7526_device_info di;

int adi_i2c_write( u8 reg, u8 *value)
{
	int ret;
	struct i2c_msg msg[2];
	u8 tmp[2]={0};

	tmp[0] = reg;
	tmp[1] = *value;

	msg[0].addr = di.client->addr;
	msg[0].len = 2;
	msg[0].flags = 0;
	msg[0].buf = tmp;
	/* over write the first byte of buffer with the register address */
	ret = i2c_transfer(di.client->adapter, msg, 1);
	/* i2c_transfer returns number of messages transferred */
	if (ret != 1) {
		pr_err("%s: i2c_write failed to transfer all messages\n",
			DRIVER_NAME);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else
		return 0;
}

int adi_i2c_read(u8 reg, u8 *value)
{
	int ret;
	struct i2c_msg msg[2];
	u8 val;

	/* [MSG1] fill the register address data */
	msg[0].addr = di.client->addr;
	msg[0].len = 1;
	msg[0].flags = 0;	/* Read the register value */
	val = reg;
	msg->buf = &val;
	/* [MSG2] fill the data rx buffer */
	msg[1].addr = di.client->addr;
	msg[1].flags = I2C_M_RD;	/* Read the register value */
	msg[1].len = 1;	/* only n bytes */
	msg[1].buf = value;
	ret = i2c_transfer(di.client->adapter, msg, 2);
	/* i2c_transfer returns number of messages transferred */
	if (ret != 2) {
		pr_err("%s: i2c_read failed to transfer all messages\n",
			DRIVER_NAME);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else
		return 0;
}

static int adi_powerup(void )
{
	int ret;
	u8 *ptr=adi_init;
	do{
		//printk("0x%02X 0x%02X\n", ptr[0], ptr[1]);
		ret = adi_i2c_write( *ptr, ptr+1);

		if( ret ){
			printk("%s, write 0x%x register failed\n", __func__, *ptr);
			return -1;
		}
		ptr += 2;
	}while(*ptr);

	return 0;
}

#ifdef IRQ

static irqreturn_t adi7526_i2c_isr(
	int irq, void *dev_id)
{
	char val = 0,reg = 0x96;
	disable_irq_nosync(di.client->irq);

	adi_i2c_read( reg, &val);
	printk("0x96 = %02X, clear interrupt\n",val);
	adi_i2c_write( reg, &val);

	//adi_i2c_read( 0x95, &val);
	//printk("0x95 = %02X,\n",val);

	queue_work(di.irq_workqueue, &di.irq_work);
	printk("%s get IRQ %d %d\n", __func__, OMAP4_ADI7526_IRQ, \
		gpio_get_value(OMAP4_ADI7526_IRQ));

	return IRQ_HANDLED;
}

static void adi7526_i2c_irq_workqueue_func(
	struct work_struct *work)
{
	printk("%s get IRQ %d %d\n", __func__, OMAP4_ADI7526_IRQ, \
		gpio_get_value(OMAP4_ADI7526_IRQ));

	enable_irq(di.client->irq);
	return ;
}
#else
static int hpd_detect(void )
{
	int ret;
	u8 val=0, reg = 0x96;
	//Read interrupt
	ret = adi_i2c_read( reg, &val);
	if( ret ){
		printk("%s, read 0x%x register failed\n", __func__, 0x96);
		return -1;
	}
	//Clear interrupt
	ret = adi_i2c_write( reg, &val);
	if( ret ){
		printk("%s, write 0x%x register failed\n", __func__, 0x96);
		return -1;
	}
	//
	reg = 0x94;
	if(val == 0x80){
		printk("HPD interrupt is detected...\n");
		val = 0x40;
		adi_i2c_write( reg, &val);//Turn on Rx sense interrupt
	}
	else if( val == 0x40 || val == 0xc0){
		//printk("Rx sense interrupt is detected...\n");
		val = 0x04;
		adi_i2c_write( reg, &val);//Turn on EDID ready interrupt
		adi_powerup();
	}
	else if( val == 0x04 ){
		printk("EDID ready interrupt is detected...\n");
		val = 0x80;
		adi_i2c_write( reg, &val);//Turn on HPD interrupt
	}
	return 0;

}
static int polling_function(void * arg)
{

	msleep(6000); //Skip blinking. Wait until Android animation is shown.
	adi_powerup();
	init_completion(&di.thread_completion);
	while (di.thread_running){
		mutex_lock(&di.adi_lock);
		hpd_detect();
		mutex_unlock(&di.adi_lock);
		msleep(1000);
	};

	complete(&di.thread_completion);

	return 0;
}

#endif

static int adi7526_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
                printk("%s, I2C_FUNC_I2C not support\n", __func__);
                return -1;
        }
	di.client = client;
        //printk("%s, i2c new style format\n", __func__);
	printk("%s, IRQ: 0x%X\n", __func__, client->irq);
	return 0;

}

static int adi7526_remove(struct i2c_client *client)
{
	mutex_destroy(&di.adi_lock);
	printk("%s\n", __func__);
	return 0;
}

static int adi7526_suspend( struct i2c_client *client, pm_message_t mesg)
{
	printk("%s, disable adi7526 irq\n", __func__);
#ifdef IRQ
	disable_irq(di.client->irq);
	flush_workqueue(di.irq_workqueue);
	mdelay(10);
#endif
	return 0;
}

static int adi7526_resume(struct i2c_client *client)
{
#ifdef IRQ
	enable_irq(di.client->irq);
#endif
	printk("%s, enable adi7526 irq\n", __func__);
	return 0;
}

/*
 * Module stuff
 */

static const struct i2c_device_id adi7526_id[] = {
	{ "adi7526", 0 },
	{},
};

static struct i2c_driver adi7526_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = adi7526_probe,
	.remove = adi7526_remove,
	.id_table = adi7526_id,
	.resume = adi7526_resume,
	.suspend  = adi7526_suspend,
};

static int __init adi7526_init(void)
{

	int ret;
	ret = i2c_add_driver(&adi7526_driver);
	if (ret)
		printk("Unable to register adi7526 driver\n");
	else{
		if(di.client == NULL){
			printk("%s, no i2c board information\n", __func__);
			return -1;
		}

		//adi_powerup();
#ifdef IRQ
		if( di.client->irq ){
			di.irq_workqueue = create_singlethread_workqueue( \
				"adi7526_i2c_irq_queue");
			if(di.irq_workqueue){
				INIT_WORK(&di.irq_work, \
					adi7526_i2c_irq_workqueue_func);

				if(request_irq(di.client->irq, adi7526_i2c_isr, \
					IRQF_TRIGGER_HIGH, \
					 "adi7526_i2c_irq", &di))
					printk("%s, request irq, error\n", __func__);
				else
		                	printk("%s, request irq, success\n", __func__);
			}
		}
#else
		mutex_init(&di.adi_lock);
		kthread_run(polling_function, NULL, "adi_polling");
		di.thread_running = 1;
#endif
	}

	return ret;

}
module_init(adi7526_init);

static void __exit adi7526_exit(void)
{
	i2c_del_driver(&adi7526_driver);
}
module_exit(adi7526_exit);

MODULE_AUTHOR("Seven Lin <sevenlin@quantatw.com>");
MODULE_DESCRIPTION("adi7526 hdmi transmitter driver");
MODULE_LICENSE("GPL");
