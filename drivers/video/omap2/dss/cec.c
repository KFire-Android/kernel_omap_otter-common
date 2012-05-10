/*
* linux/drivers/video/omap2/dss/cec.c
*
* CEC Driver
*
* Copyright (C) 2012 Texas Instruments, Inc
* Author: Muralidhar Dixit <murali.dixit@ti.com>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 as published by
* the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <video/omapdss.h>
#include <linux/switch.h>
#include <video/hdmi_ti_4xxx_ip.h>
#include <video/cec.h>
#include <linux/input/cec_keyboard.h>

#include "dss.h"
#include "cec_util.h"

#define CEC_UNREGISTERED_DEVICE	0xF
/***************************/
/* HW specific definitions */
/***************************/
/* HDMI WP base address */
#define HDMI_WP                 0x58006000

/* HDMI CORE SYSTEM base address */
#define HDMI_IP_CORE_SYSTEM	(0x400)
#define HDMI_IP_CORE_CEC	(0xD00)

#define HDMI_CORE_CEC_TIMEOUT 200

static struct cec_worker_data {
	struct delayed_work dwork;
	atomic_t state;
} cec_work;

static struct cec_t {
	struct cec_rx_data rx_data;
	struct switch_dev rx_switch;
	struct mutex lock;/* Mutex for cec access */
	struct hdmi_ip_data hdmi_data;
	struct workqueue_struct *my_workq;
	struct cec_keyboard_device key_dev;
	int phy_address;
	int device_id;/* Logical address of cec dev */
	int dss_state;
	int switch_state;
	int power_on;
	int route_ui_cmds;
	bool cec_rx_data_valid;
} cec;

static int cec_request_dss(void)
{
	cec.dss_state = dss_runtime_get();
	return cec.dss_state;
}

static void cec_release_dss(void)
{
	dss_runtime_put();
}

int cec_read_rx_cmd(struct cec_rx_data *rx_data)
{
	int r = 0;

	r = cec_request_dss();
	if (r)
		return r;

	mutex_lock(&cec.lock);
	r = hdmi_ti_4xx_cec_read_rx_cmd(&cec.hdmi_data, rx_data);
	mutex_unlock(&cec.lock);

	cec_release_dss();

	return r;
}
EXPORT_SYMBOL(cec_read_rx_cmd);

int cec_transmit_cmd(struct cec_tx_data *data, int *cmd_acked)
{
	int r = -EINVAL;

	if (data) {
		r = cec_request_dss();
		if (r)
			goto error_exit;

		mutex_lock(&cec.lock);
		r = hdmi_ti_4xx_cec_transmit_cmd(&cec.hdmi_data, data,
			cmd_acked);
		mutex_unlock(&cec.lock);

		cec_release_dss();
	}

error_exit:
	return r;
}
EXPORT_SYMBOL(cec_transmit_cmd);

int cec_register_device(struct cec_dev *dev)
{
	int acked_nacked;
	struct cec_tx_data tx_data;
	int r;
	int current_dev = 0;

	/*  1. Send an Ping command
	2. If acked, return error
	3. Register to receive for registered initiator
	4. report physical address
	5. check for nacks */

	if (dev->device_id == CEC_UNREGISTERED_DEVICE) {
		acked_nacked = 1;
		dev->phy_addr = cec.phy_address;
		pr_debug("%s Device id is unreg hence no ping required\n",
			__func__);
		goto no_ping_required;
	}
	/*send ping message to the desired device id*/
	/*Initially set initiator id to 0xf */
	tx_data.initiator_device_id = 0xF;
	tx_data.dest_device_id = dev->device_id;
	/* cmd and no of arguments are not used for ping command */
	tx_data.tx_cmd = 0x0;
	tx_data.tx_count = 0x0;

	acked_nacked = 0xFF;

	tx_data.send_ping = 0x1;
	tx_data.retry_count = 5;

	r = hdmi_ti_4xx_cec_transmit_cmd(&cec.hdmi_data, &tx_data,
		&acked_nacked);
	if (r != 0) {
		pr_err("\nCould not Ping device\n");
		return -EFAULT;
	}

no_ping_required:
	r = cec_request_dss();
	if (r)
		return r;

	current_dev = hdmi_ti_4xxx_cec_get_listening_mask(&cec.hdmi_data);

	/* Check if device is already present */
	if (acked_nacked != 1) {
		pr_debug("cec_register_device register the device\n");
		tx_data.initiator_device_id = dev->device_id;
		/* Register to receive messages intended for this device
			and broad cast messages */
		hdmi_ti_4xxx_cec_add_listening_device(&cec.hdmi_data,
			dev->device_id, dev->clear_existing_device);

		/* Report Association between physical address and device id */
		/* Report Physical address broadcast message */
		tx_data.dest_device_id = 0xF;
		tx_data.tx_cmd = cec_cmd_b_report_physical_address;
		tx_data.tx_count = 0x3;
		tx_data.tx_operand[0] = (cec.phy_address & 0xff00)>>8;
		tx_data.tx_operand[1] = cec.phy_address & 0xff;
		tx_data.tx_operand[2] = dev->device_id;
		acked_nacked = 0xFF;
		tx_data.retry_count = 5;
		tx_data.send_ping = 0;
		r = hdmi_ti_4xx_cec_transmit_cmd(&cec.hdmi_data, &tx_data,
			&acked_nacked);
		if ((acked_nacked != 1) || (r != 0x0)) {
			/* Restore previous registration */
			hdmi_ti_4xxx_cec_set_listening_mask(&cec.hdmi_data,
				current_dev);
			r = -EFAULT;
			goto err;
		}
		dev->phy_addr = cec.phy_address;
		/* Store device id for auto registration */
		cec.device_id = dev->device_id;
	} else {
		/* Device present */
		pr_info("cec_register_device device[%d] already present\n",
			dev->device_id);
		r = -EEXIST;
		goto err;
	}
err:
	cec_release_dss();
	return r;
}

int cec_ioctl_register_device(struct cec_dev *dev)
{
	int r;
	mutex_lock(&cec.lock);
	r = cec_register_device(dev);
	mutex_unlock(&cec.lock);

	return r;
}
EXPORT_SYMBOL(cec_ioctl_register_device);

int cec_enable_ui_event(int enable)
{
	cec.route_ui_cmds = enable;
	return 0;
}
EXPORT_SYMBOL(cec_enable_ui_event);

long cec_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int r;
	if (cec.power_on == 0) {
		pr_err("HDMI not connected ++\n");
		return -EFAULT;
	}
	switch (cmd) {
	case CEC_REGISTER_DEVICE:
	{
		struct cec_dev dev;

		r = copy_from_user(&dev, argp, sizeof(struct cec_dev));
		if (!r)
			r = cec_ioctl_register_device(&dev);

		if (!r)
			r = copy_to_user(argp, &dev, sizeof(struct cec_dev));

		return r == 0 ? 0 : -EFAULT;
	}
	case CEC_TRANSMIT_CMD:
	{
		struct cec_tx_data tx_cmd;
		int cmd_acked;
		r = copy_from_user(&tx_cmd, argp, sizeof(struct cec_tx_data));
		if (!r)
			r = cec_transmit_cmd(&tx_cmd, &cmd_acked);
		return r == 0 ? cmd_acked : -EFAULT;

	}
	case CEC_RECV_CMD:
	{
		struct cec_rx_data rx_cmd;
		r = cec_read_rx_cmd(&rx_cmd);
		if (!r)
			r = copy_to_user(argp, &rx_cmd,
				sizeof(struct cec_rx_data));
		return r == 0 ? 0 : -EFAULT;
	}
	case CEC_GET_PHY_ADDR:
	{

		if (cec.power_on) {
			r = copy_to_user(argp, &cec.phy_address, sizeof(int));
			return 0;
		} else
			return -EFAULT;
	}
	default:
		return -ENOTTY;
	} /* End switch */
}

/******************************************************************************
* CEC driver init/exit
******************************************************************************/

static const struct file_operations cec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = cec_ioctl,
};

static struct miscdevice mdev;

int cec_module_init(void)
{
	return 0;
}

static void cec_rx_worker(struct work_struct *work)
{
	int r;
	char rx_cmd;

	/*UI events should be sent to keyboard driver*/
	if (cec.route_ui_cmds && cec.key_dev.key_event) {
		r = cec_request_dss();
		if (r) {
			pr_err("CEC no clocks\n");
			return;
		}
		mutex_lock(&cec.lock);
		r = hdmi_ti_4xx_cec_get_rx_cmd(&cec.hdmi_data, &rx_cmd);
		if (rx_cmd == cec_cmd_u_user_control_pressed) {
			r = hdmi_ti_4xx_cec_read_rx_cmd(&cec.hdmi_data,
				&cec.rx_data);
			if (!r)
				r = cec.key_dev.key_event(cec.rx_data.rx_operand[0],
					true);
		} else if (rx_cmd == cec_cmd_u_user_control_released) {
			r = hdmi_ti_4xx_cec_read_rx_cmd(&cec.hdmi_data,
				&cec.rx_data);
			if (!r)
				r = cec.key_dev.key_event(cec.rx_data.rx_operand[0],
					false);
		}
		mutex_unlock(&cec.lock);
		cec_release_dss();
	}

	/* It is still OK to send event to user space for UI commands also
	 * just to inform them about the cec event*/

	cec.switch_state++;
	switch_set_state(&cec.rx_switch, cec.switch_state);
}

void cec_irq_cb(void)
{
	u32 cec_rx = 0;

	cec_rx = hdmi_ti_4xxx_cec_get_rx_int(&cec.hdmi_data);
	/*if command present in FIFO*/
	if (cec_rx) {
		/*UI events should be sent to keyboard driver*/
		__cancel_delayed_work(&cec_work.dwork);
		queue_delayed_work(cec.my_workq, &cec_work.dwork, 1);
		/*clear CEC RX interrupts*/
		hdmi_ti_4xxx_cec_clr_rx_int(&cec.hdmi_data, cec_rx);
	}

	return;
}

void cec_hpd_cb(int phy_addr, int status)
{
	pr_debug("cec_hpd_cb %d\n", status);
	mutex_lock(&cec.lock);
	cec.phy_address = phy_addr;
	/* Set it to unregisterd device id */
	cec.device_id = CEC_UNREGISTERED_DEVICE;
	mutex_unlock(&cec.lock);

	if (cec.key_dev.connect)
		cec.key_dev.connect(status);
}

void cec_power_on_cb(int status)
{
	int r;
	struct cec_dev dev;

	pr_debug("cec_power_on_cb ++\n");
	if (status) {
		r = cec_request_dss();
		if (r) {
			pr_err("Error in cec power on:Clk req failed\n");
			return;
		}

		mutex_lock(&cec.lock);
		cec.power_on = 1;

		hdmi_ti_4xxx_power_on_cec(&cec.hdmi_data);

		mutex_unlock(&cec.lock);

		cec_release_dss();
		/* Auto register to the previously registerd id */
		/* This is required incase of device suspend or
		when HDMI is disabled/enabled without HPD in such
		cases we do not expect the user to re-register
		the CEC device */
		if (cec.device_id != CEC_UNREGISTERED_DEVICE) {
			dev.device_id = cec.device_id;
			dev.clear_existing_device = 0;
			dev.phy_addr = cec.phy_address;
			r = cec_ioctl_register_device(&dev);
			if (r)
				pr_err("CEC power on failed to auto reg dev\n");
		}
	} else {
		mutex_lock(&cec.lock);
		cec.power_on = 0;
		mutex_unlock(&cec.lock);
	}

	pr_debug("cec_power_on_cb--\n");

	return;
}
static int __init cec_init(void)
{
	int r = -EFAULT;
	pr_debug("cec_init() %u", jiffies_to_msecs(jiffies));

	/* Map HDMI WP address */

	/* Base address taken from platform */
	cec.hdmi_data.base_wp = ioremap(HDMI_WP, 0x1000);
	pr_debug("hdmi_wp_addr=%p\n", cec.hdmi_data.base_wp);
	if (!cec.hdmi_data.base_wp) {
		pr_err("CEC: HDMI WP IOremap error\n");
		return -EFAULT;
	}

	cec.hdmi_data.hdmi_core_sys_offset = HDMI_IP_CORE_SYSTEM;
	cec.hdmi_data.hdmi_cec_offset = HDMI_IP_CORE_CEC;

	cec.cec_rx_data_valid = 0;
	cec.power_on = 0;
	/* Set it to unregisterd device id */
	cec.device_id = CEC_UNREGISTERED_DEVICE;
	mdev.minor = MISC_DYNAMIC_MINOR;
	mdev.name = "cec";
	mdev.mode = 0666;
	mdev.fops = &cec_fops;

	if (misc_register(&mdev)) {
		pr_err("CEC: Could not add character driver\n");
		goto err_register;
	}
	pr_debug("register call backs\n");
	omapdss_hdmi_register_cec_callbacks(&cec_power_on_cb, &cec_irq_cb,
		&cec_hpd_cb);
	mutex_init(&cec.lock);
	cec.switch_state = 0;
	cec.rx_switch.name = "hdmi_cec";
	r = switch_dev_register(&cec.rx_switch);
	if (r)
		goto error_event;

	cec.my_workq = create_singlethread_workqueue("cec");
	if (!cec.my_workq)
		goto error_work;

	INIT_DELAYED_WORK(&cec_work.dwork, cec_rx_worker);
	r = cec_get_keyboard_handle(&cec.key_dev);
	if (r)
		pr_err("Keyboard driver not present\n");

	cec.route_ui_cmds = 0;
	return 0;

error_work:
	switch_dev_unregister(&cec.rx_switch);
error_event:
	misc_deregister(&mdev);
	mutex_destroy(&cec.lock);
	omapdss_hdmi_unregister_cec_callbacks();
err_register:
	iounmap(cec.hdmi_data.base_wp);
	return r;
}

/*-----------------------------------------------------------------------------
 * Function: cec_exit
 *-----------------------------------------------------------------------------
 */
static void __exit cec_exit(void)
{
	pr_debug("cec_exit() %u", jiffies_to_msecs(jiffies));

	misc_deregister(&mdev);
	switch_dev_unregister(&cec.rx_switch);
	mutex_destroy(&cec.lock);
	destroy_workqueue(cec.my_workq);
	omapdss_hdmi_unregister_cec_callbacks();
	/* Unmap HDMI WP / DESHDCP */
	iounmap(cec.hdmi_data.base_wp);
}

/*-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */
module_init(cec_init);
module_exit(cec_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OMAP CEC kernel module");
MODULE_AUTHOR("Muralidhar Dixit");
