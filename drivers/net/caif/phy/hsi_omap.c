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

struct cfhsi_omap {
	struct list_head list;
	struct cfhsi_dev dev;
	struct platform_device pdev;
	struct hsi_device *hsi_dev;
	int tx_len;
	int rx_len;
	bool awake;
	unsigned int default_clock;
	unsigned int current_clock;
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
	tx_conf.divisor = 1; /* divide by 2. */
	tx_conf.arb_mode = 0; /* HSI_ARBMODE_RR. */

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
			printk(KERN_WARNING "%s: orphan.\n", __func__);
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
