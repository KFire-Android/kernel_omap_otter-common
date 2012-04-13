/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Daniel Martensson / Daniel.Martensson@stericsson.com
 * License terms: GNU General Public License (GPL) version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>

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

const u32 tx_dyn_speed_table[] = TX_DYN_SPEED_TABLE;
const u32 rx_dyn_speed_table[] = RX_DYN_SPEED_TABLE;

struct dyn_speed {
	u8 tx_speed_index;
	u8 rx_speed_index;
	u32 *tx_speed_table;
	u32 *rx_speed_table;
};

struct cfhsi_omap {
	struct list_head list;
	struct cfhsi_dev dev;
	struct platform_device pdev;
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

static bool sw_reset_on_cfhsi_up;
module_param(sw_reset_on_cfhsi_up, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sw_reset_on_cfhsi_up, "Perform software reset of HSI on interface up");

/* TODO: Lists are not protected with regards to device removal. */
static LIST_HEAD(cfhsi_dev_list);

static void cfhsi_omap_read_cb(struct hsi_device *dev, unsigned int size);
static void cfhsi_omap_write_cb(struct hsi_device *dev, unsigned int size);
static void cfhsi_omap_port_event_cb(struct hsi_device *dev,
				     unsigned int event, void *arg);
static struct hsi_device_driver cfhsi_omap_driver;

static int cfhsi_up(struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi;
	struct hst_ctx tx_conf;
	struct hsr_ctx rx_conf;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	/* Register protocol driver for HSI channel 0. */
	printk(KERN_ERR "- registering HSI driver\n");
	res =  hsi_register_driver(&cfhsi_omap_driver);
	if (res) {
		printk(KERN_ERR "%s: Failed to register CAIF HSI OMAP driver: %d.\n.",
		       __func__, res);
		return res;
	}

	/* If hsi_proto_probe() has not been called, we bail out */
	if (cfhsi->hsi_dev == NULL) {
		printk(KERN_ERR "%s: HSI device not yet present\n", __func__);
		return -ENODEV;
	}

	/* Setup callback functions. */
	hsi_set_read_cb(cfhsi->hsi_dev, cfhsi_omap_read_cb);
	hsi_set_write_cb(cfhsi->hsi_dev, cfhsi_omap_write_cb);
	hsi_set_port_event_cb(cfhsi->hsi_dev, cfhsi_omap_port_event_cb);

	/* Open OMAP HSI device. */
	res = hsi_open(cfhsi->hsi_dev);
	if (res) {
		dev_err(&cfhsi->pdev.dev,
			"%s: Failed to open HSI device: %d.\n",
			__func__, res);
		return res;
	}

	if (sw_reset_on_cfhsi_up) {
		/* HACK!!! Flush the FIFO */
		res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SW_RESET, NULL);
		if (res) {
			dev_err(&cfhsi->pdev.dev,
				"%s: Failed to reset HSI block: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}

		/* Open OMAP HSI device again after reset. */
		res = hsi_open(cfhsi->hsi_dev);
		if (res) {
			dev_err(&cfhsi->pdev.dev,
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
	tx_conf.divisor = 0; /* divide by 1. */
	tx_conf.arb_mode = 0; /* HSI_ARBMODE_RR. */
	cfhsi->tx_conf = tx_conf;

	/* CAIF HSI RX configuration. */
	rx_conf.mode = HSI_MODE_FRAME;
	rx_conf.flow = HSI_FLOW_PIPELINED;
	rx_conf.frame_size = 31;
	rx_conf.channels = 2;
	rx_conf.divisor = 0;
	rx_conf.counters =
		(180 << HSI_COUNTERS_FT_OFFSET) |
		(7 << HSI_COUNTERS_TB_OFFSET) |
		(HSI_COUNTERS_FB_DEFAULT);
	cfhsi->rx_conf = rx_conf;

	/* Configure HSI TX. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_TX, &tx_conf);
	if (res) {
		dev_err(&cfhsi->pdev.dev,
			"%s: Failed to configure TX: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}

	/* Configure HSI RX. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_RX, &rx_conf);
	if (res) {
		dev_err(&cfhsi->pdev.dev,
			"%s: Failed to configure RX: %d.\n",
			__func__, res);
		hsi_close(cfhsi->hsi_dev);
		return res;
	}

	/* Store default HSI clock */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_SPEED,
			&cfhsi->default_clock);
	if (res) {
		dev_err(&cfhsi->pdev.dev,
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
		dev_err(&cfhsi->pdev.dev,
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

static int cfhsi_down(struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

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
			dev_err(&cfhsi->pdev.dev, "%s: Wake down failed: %d.\n",
				__func__, res);
		}
	}

	/* Set default HSI clock */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SET_HI_SPEED,
			&cfhsi->default_clock);
	if (WARN_ON(res)) {
		dev_err(&cfhsi->pdev.dev,
			"%s: Failed to set HSI clock: %d.\n",
			__func__, res);
	}

	/* Reset the HSI block; set AC_DATA and AC_FLAG low */
	WARN_ON(hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_SW_RESET, NULL));

	/* Unregister OMAP-HSI driver. */
	printk(KERN_ERR "- UNregistering HSI driver\n");
	hsi_unregister_driver(&cfhsi_omap_driver);
	cfhsi->hsi_dev = NULL;

	return 0;
}

static int cfhsi_tx(u8 *ptr, int len, struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Check length and alignment. */
	BUG_ON(((int)ptr)%4);
	BUG_ON(len%4);
	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	cfhsi->tx_len = len/4;

	/* Write on HSI device. */
	res = hsi_write(cfhsi->hsi_dev, (u32 *)ptr, cfhsi->tx_len);

	return res;
}

static int cfhsi_rx(u8 *ptr, int len, struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Check length and alignment. */
	BUG_ON(((int)ptr) % 4);
	if (WARN_ON(len % 4))
		return -EINVAL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	cfhsi->rx_len = len/4;

	/* Write on HSI device. */
	res = hsi_read(cfhsi->hsi_dev, (u32 *)ptr, cfhsi->rx_len);

	return res;
}

static int cfhsi_wake_up(struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;
	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_UP, NULL);
	if (res)
		dev_err(&cfhsi->pdev.dev, "%s: Wake up failed: %d.\n",
			__func__, res);
	else
		cfhsi->awake = true;
	return res;
}

static int cfhsi_wake_down(struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_DOWN, NULL);
	if (res)
		dev_err(&cfhsi->pdev.dev, "%s: Wake down failed: %d.\n",
			__func__, res);
	else
		cfhsi->awake = false;

	return res;
}

static int cfhsi_get_peer_wake(struct cfhsi_dev *dev, bool *status)
{
	int res;
	struct cfhsi_omap *cfhsi;
	u32 wake_status;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	/* Get CA WAKE line status. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_CAWAKE, &wake_status);
	if (res < 0) {
		dev_err(&cfhsi->pdev.dev, "%s: Get CA Wake failed: %d.\n",
			__func__, res);
		return res;
	}

	if (wake_status)
		*status = true;
	else
		*status = false;

	return 0;
}

static int cfhsi_fifo_occupancy(struct cfhsi_dev *dev, size_t *occupancy)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_FIFO_OCCUPANCY,
			occupancy);
	if (res)
		dev_err(&cfhsi->pdev.dev,
			"%s: HSI_IOCTL_GET_FIFO_OCCUPANCY failed: %d.\n",
			__func__, res);

	/* Convert words to bytes */
	*occupancy *= 4;
	return res;
}

static int cfhsi_rx_cancel(struct cfhsi_dev *dev)
{
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	BUG_ON(cfhsi->hsi_dev == NULL);

	/* Cancel outstanding read request. Zero means transfer is done. */
	if (!hsi_read_cancel(cfhsi->hsi_dev))
		cfhsi->dev.drv->rx_done_cb(cfhsi->dev.drv);

	return 0;
}

static void cfhsi_omap_read_cb(struct hsi_device *dev, unsigned int size)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;

	/*
	 * TODO: It would be nice to map this without having to search through
	 * the list.
	 */
	list_for_each(list_node, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Find the corresponding device. */
		if (cfhsi->hsi_dev == dev) {
			/* BUG_ON(size != cfhsi->rx_len); */
			BUG_ON(!cfhsi->dev.drv);
			BUG_ON(!cfhsi->dev.drv->rx_done_cb);
			BUG_ON(cfhsi->hsi_dev == NULL);

			cfhsi->dev.drv->rx_done_cb(cfhsi->dev.drv);
			break;
		}
	}
}

static void cfhsi_omap_write_cb(struct hsi_device *dev, unsigned int size)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;

	/*
	 * TODO: It would be nice to map this without having to search through
	 * the list.
	 */
	list_for_each(list_node, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Find the corresponding device. */
		if (cfhsi->hsi_dev == dev) {
			/*BUG_ON(size != cfhsi->tx_len); */
			BUG_ON(!cfhsi->dev.drv);
			BUG_ON(!cfhsi->dev.drv->tx_done_cb);
			BUG_ON(cfhsi->hsi_dev == NULL);

			cfhsi->dev.drv->tx_done_cb(cfhsi->dev.drv);
			break;
		}
	}
}

static void cfhsi_omap_port_event_cb(struct hsi_device *dev,
				     unsigned int event, void *arg)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;

	/*
	 * TODO: It would be nice to map this without having to search through
	 * the list.
	 */
	list_for_each(list_node, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Find the corresponding device. */
		if (cfhsi->hsi_dev == dev)
			break;
	}

	if ((!cfhsi) || (cfhsi->hsi_dev != dev)) {
		dev_err(&dev->device, "%s (%d): No device.\n",
			__func__, event);
		return;
	}

	switch (event) {
	case HSI_EVENT_CAWAKE_UP:
		if (cfhsi->dev.drv->wake_up_cb)
			cfhsi->dev.drv->wake_up_cb(cfhsi->dev.drv);
		break;
	case HSI_EVENT_CAWAKE_DOWN:
		if (cfhsi->dev.drv->wake_down_cb)
			cfhsi->dev.drv->wake_down_cb(cfhsi->dev.drv);
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
		next_divisor = 2;
		break;
	case MIDDLE_SPEED:
		next_divisor = 1;
		break;
	case HIGH_SPEED:
		next_divisor = 0;
		break;
	default:
		dev_err(&cfhsi->pdev.dev,
			"%s: Invalid speed requested\n",
			__func__);
		return -1;
	}

	if (next_divisor != cfhsi->tx_conf.divisor) {
		cfhsi->tx_conf.divisor = next_divisor;
		/* Configure HSI TX. */
		res = hsi_ioctl(cfhsi->hsi_dev,
				HSI_IOCTL_SET_TX,
				&cfhsi->tx_conf);
		if (res) {
			dev_err(&cfhsi->pdev.dev,
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
		next_divisor = 2;
		break;
	case MIDDLE_SPEED:
		next_divisor = 1;
		break;
	case HIGH_SPEED:
		next_divisor = 0;
		break;
	default:
		dev_err(&cfhsi->pdev.dev,
			"%s: Invalid speed requested\n",
			__func__);
		return -1;
	}

	if (next_divisor != cfhsi->rx_conf.divisor) {
		cfhsi->rx_conf.divisor = next_divisor;
		/* Configure HSI RX. */
		res = hsi_ioctl(cfhsi->hsi_dev,
				HSI_IOCTL_SET_RX,
				&cfhsi->rx_conf);
		if (res) {
			dev_err(&cfhsi->pdev.dev,
				"%s: Failed to configure RX: %d.\n",
				__func__, res);
			hsi_close(cfhsi->hsi_dev);
			return res;
		}
	}

	return 0;
}

int cfhsi_change_tx_speed(struct cfhsi_dev *dev,
			  enum dyn_speed_cmd speed_cmd,
			  u32 *tx_speed,
			  enum dyn_speed_level *speed_level)
{
	struct cfhsi_omap *cfhsi = NULL;
	u8 tmp_index = 0;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	dev_dbg(&cfhsi->pdev.dev, "enter %s\n", __func__);

	BUG_ON(cfhsi->dev.drv == NULL);

	switch (speed_cmd) {
	case CFHSI_DYN_SPEED_GO_LOWEST:
		cfhsi->dyn_speed.tx_speed_index = 0;
		*speed_level = CFHSI_DYN_SPEED_LOW;
		break;
	case CFHSI_DYN_SPEED_GO_DOWN:
		*speed_level = CFHSI_DYN_SPEED_MIDDLE;
		if (cfhsi->dyn_speed.tx_speed_index > 0)
			cfhsi->dyn_speed.tx_speed_index--;
		if (cfhsi->dyn_speed.tx_speed_index == 0)
			*speed_level = CFHSI_DYN_SPEED_LOW;
		break;
	case CFHSI_DYN_SPEED_GO_UP:
		*speed_level = CFHSI_DYN_SPEED_MIDDLE;
		if (cfhsi->dyn_speed.tx_speed_index < (MAX_DYN_TX_SPEEDS-1))
			cfhsi->dyn_speed.tx_speed_index++;
		if (cfhsi->dyn_speed.tx_speed_index == (MAX_DYN_TX_SPEEDS-1))
			*speed_level = CFHSI_DYN_SPEED_HIGH;
		break;
	case CFHSI_DYN_SPEED_GO_HIGHEST:
		cfhsi->dyn_speed.tx_speed_index = (MAX_DYN_TX_SPEEDS-1);
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

int cfhsi_get_next_tx_speed(struct cfhsi_dev *dev,
			    enum dyn_speed_cmd speed_cmd,
			    u32 *tx_next_speed)
{
	u8 tmp_index;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	dev_dbg(&cfhsi->pdev.dev, "enter %s\n", __func__);

	BUG_ON(cfhsi->dev.drv == NULL);

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
		if (cfhsi->dyn_speed.tx_speed_index < (MAX_DYN_TX_SPEEDS-1))
			tmp_index++;
		break;
	case CFHSI_DYN_SPEED_GO_HIGHEST:
		tmp_index = (MAX_DYN_TX_SPEEDS-1);
		break;
	default:
		return -1;
	}

	*tx_next_speed = cfhsi->dyn_speed.tx_speed_table[tmp_index];
	return 0;
}

int cfhsi_change_rx_speed(struct cfhsi_dev *dev, u32 requested_rx_speed)
{
	struct cfhsi_omap *cfhsi = NULL;
	u32 rx_speed = 0;
	u8 tmp_index = 0;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	dev_dbg(&cfhsi->pdev.dev, "enter %s\n", __func__);

	BUG_ON(cfhsi->dev.drv == NULL);

	if (requested_rx_speed <= (LOW_SPEED+2))
		cfhsi->dyn_speed.rx_speed_index = 0;
	else if ((requested_rx_speed > (LOW_SPEED+2))
		 && (requested_rx_speed <= (MIDDLE_SPEED+4)))
		cfhsi->dyn_speed.rx_speed_index = 1;
	else if ((requested_rx_speed >= HIGH_SPEED)
		 && (requested_rx_speed <= (HIGH_SPEED+8)))
		cfhsi->dyn_speed.rx_speed_index = 2;
	else {
		printk(KERN_ERR "hsi_omap: Invalid rx speed requested=%d\n",
		       requested_rx_speed);
		return -1;
	}

	tmp_index = cfhsi->dyn_speed.rx_speed_index;
	rx_speed = cfhsi->dyn_speed.rx_speed_table[tmp_index];

	if (cfhsi_set_rx_speed(cfhsi, rx_speed) < 0)
		return -1;

	return 0;
}

static void hsi_proto_release(struct device *dev)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;
	struct list_head *n;

	list_for_each_safe(list_node, n, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Find the corresponding device. */
		if (&cfhsi->pdev.dev == dev) {
			/* This should not happen. */
			dev_warn(&cfhsi->pdev.dev, "%s: orphan.\n", __func__);
			/* Remove from list. */
			list_del(list_node);
			/* Free memory. */
			kfree(cfhsi);
			return;
		}
	}
}

static struct cfhsi_omap *cfhsi_singleton;
static int hsi_proto_probe(struct hsi_device *dev)
{
	BUG_ON(cfhsi_singleton == NULL);
	cfhsi_singleton->hsi_dev = dev;
	/* Use channel number as id. Assuming only one HSI port. */
	cfhsi_singleton->pdev.id = dev->n_ch;
	return 0;
}

int cfhsi_create_and_register(void)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;
	cfhsi = kzalloc(sizeof(struct cfhsi_omap), GFP_KERNEL);
	cfhsi_singleton = cfhsi;
	/* Assign OMAP HSI device to this CAIF HSI device. */
	cfhsi->dev.cfhsi_tx = cfhsi_tx;
	cfhsi->dev.cfhsi_rx = cfhsi_rx;
	cfhsi->dev.cfhsi_up = cfhsi_up;
	cfhsi->dev.cfhsi_down = cfhsi_down;
	cfhsi->dev.cfhsi_wake_up = cfhsi_wake_up;
	cfhsi->dev.cfhsi_wake_down = cfhsi_wake_down;
	cfhsi->dev.cfhsi_get_peer_wake = cfhsi_get_peer_wake;
	cfhsi->dev.cfhsi_fifo_occupancy = cfhsi_fifo_occupancy;
	cfhsi->dev.cfhsi_rx_cancel = cfhsi_rx_cancel;
	cfhsi->dev.cfhsi_change_tx_speed = cfhsi_change_tx_speed;
	cfhsi->dev.cfhsi_get_next_tx_speed = cfhsi_get_next_tx_speed;
	cfhsi->dev.cfhsi_change_rx_speed = cfhsi_change_rx_speed;

	/* Initialize CAIF HSI platform device. */
	cfhsi->pdev.name = "cfhsi";
	cfhsi->pdev.dev.platform_data = &cfhsi->dev;
	cfhsi->pdev.dev.release = hsi_proto_release;

	/* Add HSI device to device list. */
	list_add_tail(&cfhsi->list, &cfhsi_dev_list);

	/* Register platform device. */
	res = platform_device_register(&cfhsi->pdev);
	if (res) {
		printk(KERN_INFO "%s: Failed to register dev: %d.\n",
			__func__, res);
		res = -ENODEV;
		list_del(&cfhsi->list);
		kfree(cfhsi);
	}

	return res;
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
	printk(KERN_INFO "%s - create platform device\n", __func__);

	/* TODO: Wait for second channel. | (1 << 1)*/
	if (cpu_is_omap54xx())
		cfhsi_omap_driver.ch_mask[1] = (1 << 0);
	else
		cfhsi_omap_driver.ch_mask[0] = (1 << 0);

	return cfhsi_create_and_register();
}

static void __exit cfhsi_omap_exit(void)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;
	struct list_head *n;
	bool unregistered = false;

	list_for_each_safe(list_node, n, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		if (!unregistered) {
			/* Unregister driver. */
			printk(KERN_ERR "- UNregistering HSI driver on exit\n");
			cfhsi_down(cfhsi);
			unregistered = true;
		}
		/* Remove from list. */
		list_del(list_node);
		/* unregister CAIF HSI device. */
		platform_device_unregister(&cfhsi->pdev);
		/* FIXME: Remove this kfree, replaced with a dev_put() */
		kfree(cfhsi);
	}
}

module_init(cfhsi_omap_init);
module_exit(cfhsi_omap_exit);
