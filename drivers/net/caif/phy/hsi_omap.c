/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Daniel Martensson / Daniel.Martensson@stericsson.com
 * License terms: GNU General Public License (GPL) version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <net/caif/caif_hsi.h>
#include <linux/hsi_driver_if.h>
#include <plat/omap_hsi.h>
#include <plat/cpu.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Martensson<daniel.martensson@stericsson.com>");
MODULE_DESCRIPTION("CAIF HSI OMAP device");

#define MAX_DYN_TX_SPEEDS 3
#define MAX_DYN_RX_SPEEDS 3
#define LOW_SPEED 48
#define MIDDLE_SPEED 96
#define HIGH_SPEED 192

#define TX_DYN_SPEED_TABLE {LOW_SPEED, MIDDLE_SPEED, HIGH_SPEED}
#define RX_DYN_SPEED_TABLE {LOW_SPEED, MIDDLE_SPEED, HIGH_SPEED}

#define DIV_BY_1 0
#define DIV_BY_2 1
#define DIV_BY_4 3

const u32 tx_dyn_speed_table[] = TX_DYN_SPEED_TABLE;
const u32 rx_dyn_speed_table[] = RX_DYN_SPEED_TABLE;

struct dyn_speed {
	u8 tx_speed_index;
	u8 rx_speed_index;
	u32 *tx_speed_table;
	u32 *rx_speed_table;
};

struct cfhsi_omap {
	struct cfhsi_ops ops;
	struct hsi_device *hsi_dev;
	struct dyn_speed dyn_speed;
	int tx_len;
	int rx_len;
	bool awake;
	unsigned int default_clock;
	unsigned int current_clock;
	struct hst_ctx tx_conf;
	struct hsr_ctx rx_conf;
};

enum tx_rx_config {
	TX_CONFIG,
	RX_CONFIG
};
static struct cfhsi_omap *cfhsi_singleton;
static bool sw_reset_on_cfhsi_up;
module_param(sw_reset_on_cfhsi_up, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sw_reset_on_cfhsi_up, "Perform software reset of HSI on interface up");

static void cfhsi_omap_read_cb(struct hsi_device *dev, unsigned int size);
static void cfhsi_omap_write_cb(struct hsi_device *dev, unsigned int size);
static void cfhsi_omap_port_event_cb(struct hsi_device *dev,
				     unsigned int event, void *arg);
static struct hsi_device_driver cfhsi_omap_driver;

static struct cfhsi_omap *cfhsi_dev;

static int cfhsi_up(struct cfhsi_ops *ops)
{
	int res;
	struct cfhsi_omap *cfhsi;
	struct hst_ctx tx_conf;
	struct hsr_ctx rx_conf;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);

	/* Register protocol driver for HSI channel 0. */
	pr_info("%s: registering HSI driver\n", __func__);

	res =  hsi_register_driver(&cfhsi_omap_driver);
	if (res) {
		pr_err("%s: Failed to register CAIF HSI OMAP driver: %d.\n.",
		       __func__, res);
		return res;
	}

	/* If hsi_proto_probe() has not been called, we bail out */
	if (cfhsi->hsi_dev == NULL) {
		pr_err("%s: HSI device not yet present\n", __func__);
		return -ENODEV;
	}

	/* Setup callback functions. */
	hsi_set_read_cb(cfhsi->hsi_dev, cfhsi_omap_read_cb);
	hsi_set_write_cb(cfhsi->hsi_dev, cfhsi_omap_write_cb);
	hsi_set_port_event_cb(cfhsi->hsi_dev, cfhsi_omap_port_event_cb);

	/* Open OMAP HSI device. */
	res = hsi_open(cfhsi->hsi_dev);
	if (res) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to open HSI device: %d.\n",
			__func__, res);
		return res;
	}

	if (sw_reset_on_cfhsi_up) {
		/* HACK!!! Flush the FIFO */
		res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SW_RESET, NULL);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device,
				"%s: Failed to reset HSI block: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}

		/* Open OMAP HSI device again after reset. */
		res = hsi_open(cfhsi->hsi_dev);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device,
				"%s: Failed to open HSI device: %d.\n",
				__func__, res);
			return res;
		}
	}

	/* CAIF HSI TX configuration. */
	tx_conf.mode = HSI_MODE_FRAME;
	tx_conf.flow = HSI_FLOW_SYNCHRONIZED;
	tx_conf.frame_size = 31;
	tx_conf.channels = 2;
	tx_conf.divisor = DIV_BY_1; /* divide by 1. */
	tx_conf.arb_mode = 0; /* HSI_ARBMODE_RR. */
	cfhsi->tx_conf = tx_conf;

	/* CAIF HSI RX configuration. */
	rx_conf.mode = HSI_MODE_FRAME;
	rx_conf.flow = HSI_FLOW_PIPELINED;
	rx_conf.frame_size = 31;
	rx_conf.channels = 2;
	rx_conf.divisor = DIV_BY_1;
	rx_conf.counters =
		(180 << HSI_COUNTERS_FT_OFFSET) |
		(7 << HSI_COUNTERS_TB_OFFSET) |
		(HSI_COUNTERS_FB_DEFAULT);
	cfhsi->rx_conf = rx_conf;

	/* Configure HSI TX. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_TX, &tx_conf);
	if (res) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to configure TX: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}

	/* Configure HSI RX. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_RX, &rx_conf);
	if (res) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to configure RX: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}

	/* Store default HSI clock */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_SPEED,
			&cfhsi->default_clock);
	if (res) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to read default clock: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}

	/* Set high speed HSI clock */
	cfhsi->current_clock = HSI_SPEED_HI_SPEED;
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_HI_SPEED,
			&cfhsi->current_clock);
	if (res) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to set HSI clock: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}

	/* Set params for Dyn Speed Change, Highest speed */
	cfhsi->dyn_speed.tx_speed_index = (MAX_DYN_TX_SPEEDS-1);
	cfhsi->dyn_speed.rx_speed_index = (MAX_DYN_RX_SPEEDS-1);
	cfhsi->dyn_speed.tx_speed_table = (u32 *)&tx_dyn_speed_table[0];
	cfhsi->dyn_speed.rx_speed_table = (u32 *)&rx_dyn_speed_table[0];

	return 0;
}

static int cfhsi_down(struct cfhsi_ops *ops)
{
	int res;
	struct cfhsi_omap *cfhsi;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);

	if (cfhsi->hsi_dev == NULL)
		return 0;

	/* Make sure we don't get called back. */
	hsi_set_read_cb(cfhsi->hsi_dev, NULL);
	hsi_set_write_cb(cfhsi->hsi_dev, NULL);
	hsi_set_port_event_cb(cfhsi->hsi_dev, NULL);

	/* Set WAKE line low if not done already. */
	if (cfhsi->awake) {
		hsi_read_cancel(cfhsi->hsi_dev);
		res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_DOWN, NULL);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device, "%s: Wake down failed: %d.\n",
				__func__, res);
		}
	}

	/* Set default HSI clock */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_HI_SPEED,
			&cfhsi->default_clock);
	if (WARN_ON(res)) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to set HSI clock: %d.\n",
			__func__, res);
	}

	/* Reset the HSI block; set AC_DATA and AC_FLAG low */
	WARN_ON(hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SW_RESET, NULL));

	/* Unregister OMAP-HSI driver. */
	pr_err("- UNregistering HSI driver\n");
	hsi_unregister_driver(&cfhsi_omap_driver);
	cfhsi->hsi_dev = NULL;

	return 0;
}

static int cfhsi_tx(u8 *ptr, int len, struct cfhsi_ops *ops)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Check length and alignment. */
	BUG_ON(((int)ptr)%4);
	BUG_ON(len%4);
	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	cfhsi->tx_len = len/4;

	/* Write on HSI device. */
	res = hsi_write(cfhsi->hsi_dev, (u32 *)ptr, cfhsi->tx_len);

	return res;
}

static int cfhsi_rx(u8 *ptr, int len, struct cfhsi_ops *ops)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Check length and alignment. */
	BUG_ON(((int)ptr) % 4);
	if (WARN_ON(len % 4))
		return -EINVAL;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	cfhsi->rx_len = len/4;

	/* Write on HSI device. */
	res = hsi_read(cfhsi->hsi_dev, (u32 *)ptr, cfhsi->rx_len);

	return res;
}

static int cfhsi_wake_up(struct cfhsi_ops *ops)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;
	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_UP, NULL);
	if (res)
		dev_err(&cfhsi->hsi_dev->device, "%s: Wake up failed: %d.\n",
			__func__, res);
	else
		cfhsi->awake = true;
	return res;
}

static int cfhsi_wake_down(struct cfhsi_ops *ops)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_DOWN, NULL);
	if (res)
		dev_err(&cfhsi->hsi_dev->device, "%s: Wake down failed: %d.\n",
			__func__, res);
	else
		cfhsi->awake = false;

	return res;
}

static int cfhsi_get_peer_wake(struct cfhsi_ops *ops, bool *status)
{
	int res;
	struct cfhsi_omap *cfhsi;
	u32 wake_status;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	/* Get CA WAKE line status. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_CAWAKE, &wake_status);
	if (res < 0) {
		dev_err(&cfhsi->hsi_dev->device, "%s: Get CA Wake failed: %d.\n",
			__func__, res);
		return res;
	}

	if (wake_status)
		*status = true;
	else
		*status = false;

	return 0;
}

static int cfhsi_fifo_occupancy(struct cfhsi_ops *ops, size_t *occupancy)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_FIFO_OCCUPANCY,
			occupancy);
	if (res)
		dev_err(&cfhsi->hsi_dev->device,
			"%s: HSI_IOCTL_GET_FIFO_OCCUPANCY failed: %d.\n",
			__func__, res);

	/* Convert words to bytes */
	*occupancy *= 4;
	return res;
}

static int cfhsi_rx_cancel(struct cfhsi_ops *ops)
{
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);
	BUG_ON(cfhsi->hsi_dev == NULL);

	/* Cancel outstanding read request. Zero means transfer is done. */
	if (!hsi_read_cancel(cfhsi->hsi_dev))
		cfhsi->ops.cb_ops->rx_done_cb(cfhsi->ops.cb_ops);

	return 0;
}

static void cfhsi_omap_read_cb(struct hsi_device *dev, unsigned int size)
{
	if (cfhsi_dev->hsi_dev == dev) {
		BUG_ON(!cfhsi_dev->ops.cb_ops);
		BUG_ON(!cfhsi_dev->ops.cb_ops->rx_done_cb);
		BUG_ON(cfhsi_dev->hsi_dev == NULL);

		cfhsi_dev->ops.cb_ops->rx_done_cb(cfhsi_dev->ops.cb_ops);
	}
}

static void cfhsi_omap_write_cb(struct hsi_device *dev, unsigned int size)
{
	if (cfhsi_dev->hsi_dev == dev) {
		BUG_ON(!cfhsi_dev->ops.cb_ops);
		BUG_ON(!cfhsi_dev->ops.cb_ops->tx_done_cb);
		BUG_ON(cfhsi_dev->hsi_dev == NULL);

		cfhsi_dev->ops.cb_ops->tx_done_cb(cfhsi_dev->ops.cb_ops);
	}
}

static void cfhsi_omap_port_event_cb(struct hsi_device *dev,
				     unsigned int event, void *arg)
{
	struct cfhsi_omap *cfhsi = cfhsi_dev;

	if ((!cfhsi) || (cfhsi->hsi_dev != dev)) {
		dev_err(&dev->device, "%s (%d): No device.\n",
			__func__, event);
		return;
	}

	switch (event) {
	case HSI_EVENT_CAWAKE_UP:
		if (cfhsi->ops.cb_ops->wake_up_cb)
			cfhsi->ops.cb_ops->wake_up_cb(cfhsi->ops.cb_ops);
		break;
	case HSI_EVENT_CAWAKE_DOWN:
		if (cfhsi->ops.cb_ops->wake_down_cb)
			cfhsi->ops.cb_ops->wake_down_cb(cfhsi->ops.cb_ops);
		break;
	case HSI_EVENT_HSR_DATAAVAILABLE:
		break;
	default:
		dev_info(&dev->device, "%s: Unrecognized event %d.\n",
			__func__, event);
		break;
	}
}

static int cfhsi_set_tx_speed(struct cfhsi_omap *cfhsi, u32 speed)
{
	int res;
	u32 next_divisor  = cfhsi->tx_conf.divisor;

	switch (speed) {
	case LOW_SPEED:
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
			next_divisor = DIV_BY_4;
		else
			next_divisor = DIV_BY_2;
		break;
	case MIDDLE_SPEED:
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
			next_divisor = DIV_BY_2;
		else
			next_divisor = DIV_BY_1;
		break;
	case HIGH_SPEED:
		if (cfhsi->current_clock == HSI_SPEED_LOW_SPEED)
			dev_err(&cfhsi->hsi_dev->device, "%s: Invalid tx speed config\n",
				__func__);

		next_divisor = DIV_BY_1;
		break;
	default:
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Invalid speed requested\n",
			__func__);
		return -1;
	}


	if (next_divisor != cfhsi->tx_conf.divisor) {
		cfhsi->tx_conf.divisor = next_divisor;
		pr_info("tx_speed=%d, tx_divisor=%d\n", speed, next_divisor);
		/* Configure HSI TX. */
		res = hsi_ioctl(cfhsi->hsi_dev,
				HSI_IOCTL_SET_TX,
				&cfhsi->tx_conf);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device,
				"%s: Failed to configure TX: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}
	}

	return 0;
}

static int cfhsi_set_rx_speed(struct cfhsi_omap *cfhsi, u32 speed)
{
	int res;
	u32 next_divisor  = cfhsi->rx_conf.divisor;

	switch (speed) {
	case LOW_SPEED:
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
			next_divisor = DIV_BY_4;
		else
			next_divisor = DIV_BY_2;
		break;
	case MIDDLE_SPEED:
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
			next_divisor = DIV_BY_2;
		else
			next_divisor = DIV_BY_1;
		break;
	case HIGH_SPEED:
		next_divisor = DIV_BY_1;
		break;
	default:
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Invalid speed requested\n",
			__func__);
		return -1;
	}

	if (next_divisor != cfhsi->rx_conf.divisor) {
		cfhsi->rx_conf.divisor = next_divisor;
		pr_info("rx_speed=%d, rx_divisor=%d\n", speed, next_divisor);
		/* Configure HSI RX. */
		res = hsi_ioctl(cfhsi->hsi_dev,
				HSI_IOCTL_SET_RX,
				&cfhsi->rx_conf);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device,
				"%s: Failed to configure RX: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}
	}

	return 0;
}

struct cfhsi_ops *cfhsi_get_ops(void)
{
	struct cfhsi_omap *cfhsi = cfhsi_dev;

	pr_info("%s enter\n", __func__);

	if (!cfhsi)
		return NULL;

	return &cfhsi->ops;
}
EXPORT_SYMBOL(cfhsi_get_ops);

int cfhsi_set_hsi_clock(struct cfhsi_ops *ops, unsigned int hsi_clock)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;
	u8 tx_index, rx_index;
	struct dyn_speed *dfs_p = NULL;
	u32 tx_divisor;
	u32 rx_divisor;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);

	dev_dbg(&cfhsi->hsi_dev->device, "enter %s\n", __func__);

	BUG_ON(cfhsi->ops.cb_ops == NULL);

	if (hsi_clock == cfhsi->current_clock) {
		pr_info("Already running with this clock rate\n");
		return 0;
	}

	tx_divisor = cfhsi->tx_conf.divisor;
	rx_divisor = cfhsi->rx_conf.divisor;
	dfs_p = &cfhsi->dyn_speed;
	tx_index = dfs_p->tx_speed_index;
	rx_index = dfs_p->rx_speed_index;

	if (hsi_clock == HSI_SPEED_HI_SPEED) {
		/* Going from Low to High clock-rate*/
		switch (dfs_p->tx_speed_table[tx_index]) {
		case LOW_SPEED:
		case MIDDLE_SPEED:
			tx_index++;
			break;
		case HIGH_SPEED:
		default:
			pr_info("%s: TX Speed config error 1\n", __func__);
			break;
		}

		switch (dfs_p->rx_speed_table[rx_index]) {
		case LOW_SPEED:
		case MIDDLE_SPEED:
			rx_index++;
			break;
		case HIGH_SPEED:
		default:
			pr_info("%s: RX Speed config error 1\n", __func__);
			break;
		}
	} else {
		/* Going from High to Low clock-rate*/
		switch (dfs_p->tx_speed_table[tx_index]) {
		case LOW_SPEED:
			tx_divisor = DIV_BY_2;
			break;
		case MIDDLE_SPEED:
			tx_divisor = DIV_BY_1;
			break;
		case HIGH_SPEED:
			tx_index--;
		default:
			pr_info("%s: TX Speed config error 2\n", __func__);
			break;
		}
		switch (dfs_p->rx_speed_table[rx_index]) {
		case LOW_SPEED:
			rx_divisor = DIV_BY_2;
			break;
		case MIDDLE_SPEED:
			rx_divisor = DIV_BY_1;
			break;
		case HIGH_SPEED:
			rx_index--;
		default:
			pr_info("%s: RX Speed config error 2\n", __func__);
			break;
		}
	}

	dfs_p->tx_speed_index = tx_index;
	dfs_p->rx_speed_index = rx_index;

	pr_info("rx_speed=%d, rx_divisor=%d\n",
		dfs_p->tx_speed_table[rx_index], rx_divisor);
	if (rx_divisor != cfhsi->rx_conf.divisor) {
		cfhsi->rx_conf.divisor = rx_divisor;

		/* Configure HSI RX. */
		res = hsi_ioctl(cfhsi->hsi_dev,
				HSI_IOCTL_SET_RX,
				&cfhsi->rx_conf);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device,
				"%s: Failed to configure RX: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}
	}

	pr_info("tx_speed=%d, tx_divisor=%d\n",
		dfs_p->rx_speed_table[tx_index], rx_divisor);
	if (tx_divisor != cfhsi->tx_conf.divisor) {
		cfhsi->tx_conf.divisor = tx_divisor;

		/* Configure HSI TX. */
		res = hsi_ioctl(cfhsi->hsi_dev,
				HSI_IOCTL_SET_TX,
				&cfhsi->tx_conf);
		if (res) {
			dev_err(&cfhsi->hsi_dev->device,
				"%s: Failed to configure TX: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}
	}

	/* Set HSI clock */
	cfhsi->current_clock = hsi_clock;
	pr_info("New clock value=%d\n", hsi_clock);
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_HI_SPEED,
			&cfhsi->current_clock);
	if (res) {
		dev_err(&cfhsi->hsi_dev->device,
			"%s: Failed to set HSI clock: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}
	return 0;
}

int cfhsi_change_tx_speed(struct cfhsi_ops *ops,
			  enum dyn_speed_cmd speed_cmd,
			  u32 *tx_speed,
			  enum dyn_speed_level *speed_level)
{
	struct cfhsi_omap *cfhsi = NULL;
	u8 tmp_index = 0;
	u8 max_index;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);

	dev_dbg(&cfhsi->hsi_dev->device, "enter %s\n", __func__);

	BUG_ON(cfhsi->ops.cb_ops == NULL);

	if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
		max_index = MAX_DYN_TX_SPEEDS-1;
	else
		max_index = MAX_DYN_TX_SPEEDS-2;

	switch (speed_cmd) {
	case CFHSI_DYN_SPEED_GO_LOWEST:
		cfhsi->dyn_speed.tx_speed_index = 0;
		*speed_level = CFHSI_DYN_SPEED_LOW;
		break;
	case CFHSI_DYN_SPEED_GO_DOWN:
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED) {
			*speed_level = CFHSI_DYN_SPEED_MIDDLE;

			if (cfhsi->dyn_speed.tx_speed_index > 0)
				cfhsi->dyn_speed.tx_speed_index--;
			if (cfhsi->dyn_speed.tx_speed_index == 0)
				*speed_level = CFHSI_DYN_SPEED_LOW;
		} else {
			if (cfhsi->dyn_speed.tx_speed_index > 0)
				cfhsi->dyn_speed.tx_speed_index--;
			*speed_level = CFHSI_DYN_SPEED_LOW;
		}
		break;
	case CFHSI_DYN_SPEED_GO_UP:
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED) {
			*speed_level = CFHSI_DYN_SPEED_MIDDLE;

			if (cfhsi->dyn_speed.tx_speed_index < max_index)
				cfhsi->dyn_speed.tx_speed_index++;
			if (cfhsi->dyn_speed.tx_speed_index == max_index)
				*speed_level = CFHSI_DYN_SPEED_HIGH;
		} else {
			if (cfhsi->dyn_speed.tx_speed_index < max_index)
				cfhsi->dyn_speed.tx_speed_index++;
			*speed_level = CFHSI_DYN_SPEED_HIGH;
		}
		break;
	case CFHSI_DYN_SPEED_GO_HIGHEST:
		cfhsi->dyn_speed.tx_speed_index = max_index;
		*speed_level = CFHSI_DYN_SPEED_HIGH;
		break;
	default:
		return -1;
	}

	tmp_index = cfhsi->dyn_speed.tx_speed_index;
	*tx_speed = cfhsi->dyn_speed.tx_speed_table[tmp_index];

	if (cfhsi_set_tx_speed(cfhsi, *tx_speed) < 0)
		return -1;

	return 0;
}

int cfhsi_get_next_tx_speed(struct cfhsi_ops *ops,
			    enum dyn_speed_cmd speed_cmd,
			    u32 *tx_next_speed)
{
	u8 tmp_index;
	struct cfhsi_omap *cfhsi = NULL;
	u8 max_index;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);

	dev_dbg(&cfhsi->hsi_dev->device, "enter %s\n", __func__);

	BUG_ON(cfhsi->ops.cb_ops == NULL);

	if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
		max_index = MAX_DYN_TX_SPEEDS-1;
	else
		max_index = MAX_DYN_TX_SPEEDS-2;

	switch (speed_cmd) {
	case CFHSI_DYN_SPEED_GO_LOWEST:
		tmp_index = 0;
		break;
	case CFHSI_DYN_SPEED_GO_DOWN:
		tmp_index = cfhsi->dyn_speed.tx_speed_index;
		if (cfhsi->dyn_speed.tx_speed_index > 0)
			tmp_index--;
		break;
	case CFHSI_DYN_SPEED_GO_UP:
		tmp_index = cfhsi->dyn_speed.tx_speed_index;
		if (cfhsi->dyn_speed.tx_speed_index < max_index)
			tmp_index++;
		break;
	case CFHSI_DYN_SPEED_GO_HIGHEST:
		tmp_index = max_index;
		break;
	default:
		return -1;
	}

	*tx_next_speed = cfhsi->dyn_speed.tx_speed_table[tmp_index];
	return 0;
}

int cfhsi_change_rx_speed(struct cfhsi_ops *ops, u32 requested_rx_speed)
{
	struct cfhsi_omap *cfhsi = NULL;
	u32 rx_speed = 0;
	u8 tmp_index = 0;

	cfhsi = container_of(ops, struct cfhsi_omap, ops);

	dev_dbg(&cfhsi->hsi_dev->device, "enter %s\n", __func__);

	BUG_ON(cfhsi->ops.cb_ops == NULL);

	if (requested_rx_speed <= (LOW_SPEED+2))
		cfhsi->dyn_speed.rx_speed_index = 0;
	else if ((requested_rx_speed > (LOW_SPEED+2))
		 && (requested_rx_speed <= (MIDDLE_SPEED+4)))
		cfhsi->dyn_speed.rx_speed_index = 1;
	else if ((requested_rx_speed >= HIGH_SPEED)
		 && (requested_rx_speed <= (HIGH_SPEED+8)))
		if (cfhsi->current_clock == HSI_SPEED_HI_SPEED)
			cfhsi->dyn_speed.rx_speed_index = 2;
		else
			cfhsi->dyn_speed.rx_speed_index = 1;
	else {
		pr_err("hsi_omap: Invalid rx speed requested=%d\n",
		       requested_rx_speed);
		return -1;
	}

	tmp_index = cfhsi->dyn_speed.rx_speed_index;
	rx_speed = cfhsi->dyn_speed.rx_speed_table[tmp_index];

	if (cfhsi_set_rx_speed(cfhsi, rx_speed) < 0)
		return -1;

	return 0;
}

static int hsi_proto_probe(struct hsi_device *dev)
{
	BUG_ON(cfhsi_singleton == NULL);
	cfhsi_singleton->hsi_dev = dev;
	/* Use channel number as id. Assuming only one HSI port. */
	return 0;
}

int cfhsi_create_and_register(void)
{
	struct cfhsi_omap *cfhsi = NULL;
	cfhsi = kzalloc(sizeof(struct cfhsi_omap), GFP_KERNEL);
	cfhsi_singleton = cfhsi;
	/* Assign OMAP HSI device to this CAIF HSI device. */
	cfhsi->ops.cfhsi_tx = cfhsi_tx;
	cfhsi->ops.cfhsi_rx = cfhsi_rx;
	cfhsi->ops.cfhsi_up = cfhsi_up;
	cfhsi->ops.cfhsi_down = cfhsi_down;
	cfhsi->ops.cfhsi_wake_up = cfhsi_wake_up;
	cfhsi->ops.cfhsi_wake_down = cfhsi_wake_down;
	cfhsi->ops.cfhsi_get_peer_wake = cfhsi_get_peer_wake;
	cfhsi->ops.cfhsi_fifo_occupancy = cfhsi_fifo_occupancy;
	cfhsi->ops.cfhsi_rx_cancel = cfhsi_rx_cancel;
	cfhsi->ops.cfhsi_set_hsi_clock = cfhsi_set_hsi_clock;
	cfhsi->ops.cfhsi_change_tx_speed = cfhsi_change_tx_speed;
	cfhsi->ops.cfhsi_get_next_tx_speed = cfhsi_get_next_tx_speed;
	cfhsi->ops.cfhsi_change_rx_speed = cfhsi_change_rx_speed;

	cfhsi_dev = cfhsi;

	return 0;
}

static int hsi_proto_remove(struct hsi_device *dev)
{
	cfhsi_singleton->hsi_dev = NULL;
	return 0;
}

static int hsi_proto_suspend(struct hsi_device *dev, pm_message_t mesg)
{
	/* Not needed for HSI driver */
	return 0;
}

static int hsi_proto_resume(struct hsi_device *dev)
{
	/* Not needed for HSI driver */
	return 0;
}

static struct hsi_device_driver cfhsi_omap_driver = {
	.ctrl_mask = ANY_HSI_CONTROLLER,
	.probe = hsi_proto_probe,
	.remove = hsi_proto_remove,
	.suspend = hsi_proto_suspend,
	.resume = hsi_proto_resume,
	.driver = {
		.name = "cfhsi_omap_driver",
	},
};

static int __init cfhsi_omap_init(void)
{
	pr_info("%s - create platform device\n", __func__);

	/* TODO: Wait for second channel. | (1 << 1)*/
	if (cpu_is_omap54xx())
		cfhsi_omap_driver.ch_mask[1] = (1 << 0);
	else
		cfhsi_omap_driver.ch_mask[0] = (1 << 0);

	return cfhsi_create_and_register();
}

static void __exit cfhsi_omap_exit(void)
{
	kfree(cfhsi_dev);
}

module_init(cfhsi_omap_init);
module_exit(cfhsi_omap_exit);
