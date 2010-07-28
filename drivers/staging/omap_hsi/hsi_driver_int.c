/*
 * hsi_driver_int.c
 *
 * Implements HSI interrupt functionality.
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

#include "hsi_driver.h"

void hsi_reset_ch_read(struct hsi_channel *ch)
{
	ch->read_data.addr = NULL;
	ch->read_data.size = 0;
	ch->read_data.lch = -1;
}

void hsi_reset_ch_write(struct hsi_channel *ch)
{
	ch->write_data.addr = NULL;
	ch->write_data.size = 0;
	ch->write_data.lch = -1;
}

int hsi_driver_write_interrupt(struct hsi_channel *ch, u32 *data)
{
	struct hsi_port *p = ch->hsi_port;
	unsigned int port = p->port_number;
	unsigned int channel = ch->channel_number;

	hsi_outl_or(HSI_HST_DATAACCEPT(channel), p->hsi_controller->base,
			HSI_SYS_MPU_ENABLE_CH_REG(port, p->n_irq, channel));

	return 0;
}

int hsi_driver_read_interrupt(struct hsi_channel *ch, u32 *data)
{
	struct hsi_port *p = ch->hsi_port;
	unsigned int port = p->port_number;
	unsigned int channel = ch->channel_number;

	hsi_outl_or(HSI_HSR_DATAAVAILABLE(channel), p->hsi_controller->base,
			HSI_SYS_MPU_ENABLE_CH_REG(port, p->n_irq, channel));

	return 0;
}

void hsi_driver_cancel_write_interrupt(struct hsi_channel *ch)
{
	struct hsi_port *p = ch->hsi_port;
	unsigned int port = p->port_number;
	unsigned int channel = ch->channel_number;
	void __iomem *base = p->hsi_controller->base;
	u32 enable;
	long buff_offset;

	enable = hsi_inl(base,
			HSI_SYS_MPU_ENABLE_CH_REG(port, p->n_irq, channel));

	if (!(enable & HSI_HST_DATAACCEPT(channel))) {
		dev_dbg(&ch->dev->device, LOG_NAME "Write cancel on not "
		"enabled channel %d ENABLE REG 0x%08X", channel, enable);
		return;
	}

	hsi_outl_and(~HSI_HST_DATAACCEPT(channel), base,
			HSI_SYS_MPU_ENABLE_CH_REG(port, p->n_irq, channel));

	buff_offset = hsi_hst_bufstate_f_reg(p->hsi_controller, port, channel);
	if (buff_offset >= 0)
		hsi_outl_and(~HSI_BUFSTATE_CHANNEL(channel), base,
								buff_offset);

	hsi_reset_ch_write(ch);
}

void hsi_driver_disable_read_interrupt(struct hsi_channel *ch)
{
	struct hsi_port *p = ch->hsi_port;
	unsigned int port = p->port_number;
	unsigned int channel = ch->channel_number;
	void __iomem *base = p->hsi_controller->base;

	hsi_outl_and(~HSI_HSR_DATAAVAILABLE(channel), base,
			HSI_SYS_MPU_ENABLE_CH_REG(port, p->n_irq, channel));
}

void hsi_driver_cancel_read_interrupt(struct hsi_channel *ch)
{
	struct hsi_port *p = ch->hsi_port;
	unsigned int port = p->port_number;
	unsigned int channel = ch->channel_number;
	void __iomem *base = p->hsi_controller->base;

	hsi_outl_and(~HSI_HSR_DATAAVAILABLE(channel), base,
			HSI_SYS_MPU_ENABLE_CH_REG(port, p->n_irq, channel));

	hsi_reset_ch_read(ch);
}

static void do_channel_tx(struct hsi_channel *ch)
{
	struct hsi_dev *hsi_ctrl = ch->hsi_port->hsi_controller;
	void __iomem *base = hsi_ctrl->base;
	unsigned int n_ch;
	unsigned int n_p;
	unsigned int irq;
	long buff_offset;

	n_ch = ch->channel_number;
	n_p = ch->hsi_port->port_number;
	irq = ch->hsi_port->n_irq;

	spin_lock(&hsi_ctrl->lock);

	if (ch->write_data.addr == NULL) {
		hsi_outl_and(~HSI_HST_DATAACCEPT(n_ch), base,
				HSI_SYS_MPU_ENABLE_CH_REG(n_p, irq, n_ch));
		hsi_reset_ch_write(ch);
		spin_unlock(&hsi_ctrl->lock);
		(*ch->write_done)(ch->dev, 1);
	} else {
		buff_offset = hsi_hst_buffer_reg(hsi_ctrl, n_p, n_ch);
		if (buff_offset >= 0) {
			hsi_outl(*(ch->write_data.addr), base, buff_offset);
			ch->write_data.addr = NULL;
		}
		spin_unlock(&hsi_ctrl->lock);
	}
}

static void do_channel_rx(struct hsi_channel *ch)
{
	struct hsi_dev *hsi_ctrl = ch->hsi_port->hsi_controller;
	void __iomem *base = ch->hsi_port->hsi_controller->base;
	unsigned int n_ch;
	unsigned int n_p;
	unsigned int irq;
	long buff_offset;
	int rx_poll = 0;
	int data_read = 0;

	n_ch = ch->channel_number;
	n_p = ch->hsi_port->port_number;
	irq = ch->hsi_port->n_irq;

	spin_lock(&hsi_ctrl->lock);

	if (ch->flags & HSI_CH_RX_POLL)
		rx_poll = 1;

	if (ch->read_data.addr) {
		buff_offset = hsi_hsr_buffer_reg(hsi_ctrl, n_p, n_ch);
		if (buff_offset >= 0) {
			data_read = 1;
			*(ch->read_data.addr) = hsi_inl(base, buff_offset);
		}
	}

	hsi_outl_and(~HSI_HSR_DATAAVAILABLE(n_ch), base,
				HSI_SYS_MPU_ENABLE_CH_REG(n_p, irq, n_ch));
	hsi_reset_ch_read(ch);

	spin_unlock(&hsi_ctrl->lock);

	if (rx_poll)
		hsi_port_event_handler(ch->hsi_port,
				HSI_EVENT_HSR_DATAAVAILABLE, (void *)n_ch);

	if (data_read)
		(*ch->read_done)(ch->dev, 1);
}

/**
 * hsi_driver_int_proc - check all channels / ports for interrupts events
 * @hsi_ctrl - HSI controler data
 * @status_offset: interrupt status register offset
 * @enable_offset: interrupt enable regiser offset
 * @start: interrupt index to start on
 * @stop: interrupt index to stop on
 *
 * This function calls the related processing functions and triggered events
*/
static void hsi_driver_int_proc(struct hsi_port *pport,
		unsigned long status_offset, unsigned long enable_offset,
		unsigned int start, unsigned int stop)
{
	struct hsi_dev *hsi_ctrl = pport->hsi_controller;
	void __iomem *base = hsi_ctrl->base;
	unsigned int port = pport->port_number;
	unsigned int channel;
	u32 status_reg;
	u32 hsr_err_reg;
	u32 channels_served = 0;

	status_reg = hsi_inl(base, status_offset);
	status_reg &= hsi_inl(base, enable_offset);

	for (channel = start; channel < stop; channel++) {
		if (status_reg & HSI_HST_DATAACCEPT(channel)) {

			do_channel_tx(&pport->hsi_channel[channel]);
			channels_served |= HSI_HST_DATAACCEPT(channel);
		}

		if (status_reg & HSI_HSR_DATAAVAILABLE(channel)) {
			do_channel_rx(&pport->hsi_channel[channel]);
			channels_served |= HSI_HSR_DATAAVAILABLE(channel);
		}
	}

	if (status_reg & HSI_BREAKDETECTED) {
		dev_info(hsi_ctrl->dev, "Hardware BREAK on port %d\n", port);
		hsi_outl(0, base, HSI_HSR_BREAK_REG(port));
		hsi_port_event_handler(pport, HSI_EVENT_BREAK_DETECTED, NULL);
		channels_served |= HSI_BREAKDETECTED;
	}

	if (status_reg & HSI_ERROROCCURED) {
		hsr_err_reg = hsi_inl(base, HSI_HSR_ERROR_REG(port));
		dev_err(hsi_ctrl->dev, "HSI ERROR Port %d: 0x%x\n",
							port, hsr_err_reg);
		hsi_outl(hsr_err_reg, base, HSI_HSR_ERRORACK_REG(port));
		if (hsr_err_reg) /* ignore spurious errors */
			hsi_port_event_handler(pport, HSI_EVENT_ERROR, NULL);
		else
			dev_dbg(hsi_ctrl->dev, "Spurious HSI error!\n");

		channels_served |= HSI_ERROROCCURED;
	}

	hsi_outl(channels_served, base, status_offset);
}

static void do_hsi_tasklet(unsigned long hsi_port)
{
	struct hsi_port *pport = (struct hsi_port *)hsi_port;
	struct hsi_dev *hsi_ctrl = pport->hsi_controller;
	void __iomem *base = hsi_ctrl->base;
	unsigned int port = pport->port_number;
	unsigned int irq = pport->n_irq;
	u32 status_reg;
	struct platform_device *pd = to_platform_device(hsi_ctrl->dev);

	hsi_driver_int_proc(pport,
			HSI_SYS_MPU_STATUS_REG(port, irq),
			HSI_SYS_MPU_ENABLE_REG(port, irq),
			0, min(pport->max_ch, (u8) HSI_SSI_CHANNELS_MAX));

	if (pport->max_ch > HSI_SSI_CHANNELS_MAX)
		hsi_driver_int_proc(pport,
				HSI_SYS_MPU_U_STATUS_REG(port, irq),
				HSI_SYS_MPU_U_ENABLE_REG(port, irq),
				HSI_SSI_CHANNELS_MAX, pport->max_ch);

	status_reg = hsi_inl(base, HSI_SYS_MPU_STATUS_REG(port, irq)) &
			hsi_inl(base, HSI_SYS_MPU_ENABLE_REG(port, irq));

	if (hsi_driver_device_is_hsi(pd))
		status_reg |=
			(hsi_inl(base, HSI_SYS_MPU_U_STATUS_REG(port, irq)) &
			hsi_inl(base, HSI_SYS_MPU_U_ENABLE_REG(port, irq)));

	if (status_reg)
		tasklet_hi_schedule(&pport->hsi_tasklet);
	else
		enable_irq(pport->irq);
}

static irqreturn_t hsi_mpu_handler(int irq, void *hsi_port)
{
	struct hsi_port *p = hsi_port;

	tasklet_hi_schedule(&p->hsi_tasklet);
	disable_irq_nosync(p->irq);

	return IRQ_HANDLED;
}

int __init hsi_mpu_init(struct hsi_port *hsi_p, const char *irq_name)
{
	int err;

	tasklet_init(&hsi_p->hsi_tasklet, do_hsi_tasklet,
							(unsigned long)hsi_p);
	err = request_irq(hsi_p->irq, hsi_mpu_handler, IRQF_DISABLED,
							irq_name, hsi_p);
	if (err < 0) {
		dev_err(hsi_p->hsi_controller->dev, "FAILED to MPU request"
			" IRQ (%d) on port %d", hsi_p->irq, hsi_p->port_number);
		return -EBUSY;
	}

	return 0;
}

void hsi_mpu_exit(struct hsi_port *hsi_p)
{
	tasklet_disable(&hsi_p->hsi_tasklet);
	free_irq(hsi_p->irq, hsi_p);
}
