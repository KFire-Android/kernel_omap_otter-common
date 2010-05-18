/*
 * hsi_driver_if.h
 *
 * Header for the HSI driver low level interface.
 *
 * Copyright (C) 2007-2008 Nokia Corporation. All rights reserved.
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author: Carlos Chinea <carlos.chinea@nokia.com>
 * Author: Sebastien JAN <s-jan@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __HSI_DRIVER_IF_H__
#define __HSI_DRIVER_IF_H__

#include <linux/device.h>
#include <linux/clk.h>
#include <linux/notifier.h>

/* The number of ports handled by the driver (MAX:2). Reducing this value
 * optimizes the driver memory footprint.
 */
#define HSI_MAX_PORTS		1

/* bit-field definition for allowed controller IDs and channels */
#define ANY_HSI_CONTROLLER	-1

/* HSR special divisor values set to control the auto-divisor Rx mode */
#define HSI_HSR_DIVISOR_AUTO		0x1000 /* Activate auto Rx */
#define HSI_SSR_DIVISOR_USE_TIMEOUT	0x1001 /* De-activate auto-Rx for SSI */

enum {
	HSI_EVENT_BREAK_DETECTED = 0,
	HSI_EVENT_ERROR,
	HSI_EVENT_PRE_SPEED_CHANGE,
	HSI_EVENT_POST_SPEED_CHANGE,
	HSI_EVENT_CAWAKE_UP,
	HSI_EVENT_CAWAKE_DOWN,
	HSI_EVENT_HSR_DATAAVAILABLE,
};

enum {
	HSI_IOCTL_WAKE_UP,
	HSI_IOCTL_WAKE_DOWN,
	HSI_IOCTL_SEND_BREAK,
	HSI_IOCTL_WAKE,
	HSI_IOCTL_FLUSH_RX,
	HSI_IOCTL_FLUSH_TX,
	HSI_IOCTL_CAWAKE,
	HSI_IOCTL_SET_RX,
	HSI_IOCTL_GET_RX,
	HSI_IOCTL_SET_TX,
	HSI_IOCTL_GET_TX,
};

/* Forward references */
struct hsi_device;
struct hsi_channel;

/* DPS */
struct hst_ctx {
	u32 mode;
	u32 flow;
	u32 frame_size;
	u32 divisor;
	u32 arb_mode;
	u32 channels;
};

struct hsr_ctx {
	u32 mode;
	u32 flow;
	u32 frame_size;
	u32 divisor;
	u32 timeout;
	u32 channels;
};

struct port_ctx {
	u32 sys_mpu_enable[2];
	struct hst_ctx hst;
	struct hsr_ctx hsr;
};

/**
 * struct ctrl_ctx - hsi controller regs context
 * @loss_count: hsi last loss count
 * @sysconfig: keeps sysconfig reg state
 * @gdd_gcr: keeps gcr reg state
 * @pctx: array of port context
 */
struct ctrl_ctx {
	int loss_count;
	u32 sysconfig;
	u32 gdd_gcr;
	struct port_ctx *pctx;
};
/* END DPS */

struct hsi_platform_data {
	void (*set_min_bus_tput)(struct device *dev, u8 agent_id,
							unsigned long r);
	int (*clk_notifier_register)(struct clk *clk,
						struct notifier_block *nb);
	int (*clk_notifier_unregister)(struct clk *clk,
						struct notifier_block *nb);
	u8 num_ports;
	struct ctrl_ctx ctx;
};

/**
 * struct hsi_device - HSI device object
 * @n_ctrl: associated HSI controller platform id number
 * @n_p: port number
 * @n_ch: channel number
 * @modalias: [to be removed]
 * @ch: channel descriptor
 * @device: associated device
*/
struct hsi_device {
	int n_ctrl;
	unsigned int n_p;
	unsigned int n_ch;
	struct hsi_channel *ch;
	struct device device;
};

#define to_hsi_device(dev)	container_of(dev, struct hsi_device, device)

/**
 * struct hsi_device_driver - HSI driver instance container
 * @ctrl_mask: bit-field indicating the supported HSI device ids
 * @ch_mask: bit-field indicating enabled channels for this port
 * @probe: probe callback (driver registering)
 * @remove: remove callback (driver un-registering)
 * @suspend: suspend callback
 * @resume: resume callback
 * @driver: associated device_driver object
*/
struct hsi_device_driver {
	unsigned long		ctrl_mask;
	unsigned long		ch_mask[HSI_MAX_PORTS];
	int			(*probe)(struct hsi_device *dev);
	int			(*remove)(struct hsi_device *dev);
	int			(*suspend)(struct hsi_device *dev,
						pm_message_t mesg);
	int			(*resume)(struct hsi_device *dev);
	struct device_driver	driver;
};

#define to_hsi_device_driver(drv) container_of(drv, \
						struct hsi_device_driver, \
						driver)

int hsi_register_driver(struct hsi_device_driver *driver);
void hsi_unregister_driver(struct hsi_device_driver *driver);
int hsi_open(struct hsi_device *dev);
int hsi_write(struct hsi_device *dev, u32 *addr, unsigned int size);
void hsi_write_cancel(struct hsi_device *dev);
int hsi_read(struct hsi_device *dev, u32 *addr, unsigned int size);
void hsi_read_cancel(struct hsi_device *dev);
int hsi_poll(struct hsi_device *dev);
int hsi_ioctl(struct hsi_device *dev, unsigned int command, void *arg);
void hsi_close(struct hsi_device *dev);
void hsi_set_read_cb(struct hsi_device *dev,
		void (*read_cb)(struct hsi_device *dev, unsigned int size));
void hsi_set_write_cb(struct hsi_device *dev,
		void (*write_cb)(struct hsi_device *dev, unsigned int size));
void hsi_set_port_event_cb(struct hsi_device *dev,
				void (*port_event_cb)(struct hsi_device *dev,
					unsigned int event, void *arg));
#endif /* __HSI_DRIVER_IF_H__ */
