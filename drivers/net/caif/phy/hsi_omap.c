/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Daniel Martensson / Daniel.Martensson@stericsson.com
 * License terms: GNU General Public License (GPL) version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <net/caif/caif_hsi.h>

#include <linux/hsi_driver_if.h>

#include <mach/omap_hsi.h>

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
};

/* TODO: Lists are not protected with regards to device removal. */
static LIST_HEAD(cfhsi_dev_list);

/* HSI configuration structure. */
struct hst_ctx tx_conf;
struct hsr_ctx rx_conf;

static int cfhsi_up (struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	/* Request the HSI.*/
	res = hsi_open(cfhsi->hsi_dev);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: hsi_open failed: %d.\n",
			__func__, res);
		return res;
	}

	/* Flush both the receiver and the transmitter. */
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_FLUSH_TX, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: TX flush failed: %d.\n",
			__func__, res);
	}

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_FLUSH_RX, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: RX flush failed: %d.\n",
			__func__, res);
	}
	return 0;
}

static int cfhsi_down (struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);
	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_FLUSH_TX, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: TX flush failed: %d.\n",
			__func__, res);
	}

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_FLUSH_RX, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: RX flush failed: %d.\n",
			__func__, res);
	}
	/* Cancel outstanding read request. */
	hsi_read_cancel(cfhsi->hsi_dev);

	/* Release the HSI. */
	hsi_close(cfhsi->hsi_dev);

	return 0;
}

static int cfhsi_tx (u8 *ptr, int len, struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Check length and alignment. */
	BUG_ON(((int)ptr)%4);
	BUG_ON(len%4);

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	cfhsi->tx_len = len/4;

	/* Write on HSI device. */
	res = hsi_write(cfhsi->hsi_dev, (u32 *)ptr, cfhsi->tx_len);

	return res;
}

static int cfhsi_rx (u8 *ptr, int len, struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Check length and alignment. */
	BUG_ON(((int)ptr) % 4);
	if (WARN_ON(len % 4))
		return -EINVAL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

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

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_UP, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: Wake up failed: %d.\n",
			__func__, res);
	}
	return res;
}

static int cfhsi_wake_down(struct cfhsi_dev *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_DOWN, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: Wake down failed: %d.\n",
			__func__, res);
	}

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_FLUSH_RX, NULL);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: RX flush failed: %d.\n",
			__func__, res);
	}

	return res;
}

static int cfhsi_fifo_occupancy(struct cfhsi_dev *dev, size_t *occupancy)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	cfhsi = container_of(dev, struct cfhsi_omap, dev);

	res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_GET_FIFO_OCCUPANCY, occupancy);
	if (res) {
		dev_err(&cfhsi->pdev.dev, "%s: HSI_IOCTL_GET_FIFO_OCCUPANCY failed: %d.\n",
			__func__, res);
	}

	return res;
}

static inline void cfhsi_omap_read_cb(struct hsi_device *dev, unsigned int size)
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

			cfhsi->dev.drv->rx_done_cb(cfhsi->dev.drv);
			break;
		}
	}
}

static inline void cfhsi_omap_write_cb(struct hsi_device *dev, unsigned int size)
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
			/* BUG_ON(size != cfhsi->tx_len); */
			BUG_ON(!cfhsi->dev.drv);
			BUG_ON(!cfhsi->dev.drv->tx_done_cb);

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
		if (cfhsi->hsi_dev == dev) {
			break;
		}
	}

	if ((!cfhsi) || (cfhsi->hsi_dev != dev)) {
		dev_err(&dev->device, "%s: No device.\n",
			__func__);
		return;
	}

	switch (event) {
	case HSI_EVENT_CAWAKE_UP:
		cfhsi->dev.drv->wake_up_cb(cfhsi->dev.drv);
		break;
	case HSI_EVENT_CAWAKE_DOWN:
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
	int res;

	list_for_each_safe(list_node, n, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Find the corresponding device. */
		if (&cfhsi->pdev.dev == dev) {
			/* Remove from list. */
			list_del(list_node);

			/*TODO: We should check if the device is open or not. */
			res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_DOWN, NULL);
			if (res) {
				printk(KERN_WARNING "%s: failed to set WAKE: %d\n",
						__func__, res);
			}
			hsi_close(cfhsi->hsi_dev);
			/* Free memory. */
			kfree(cfhsi);
			return;
		}
	}
}

static int hsi_proto_probe(struct hsi_device *dev)
{
	int res;
	struct cfhsi_omap *cfhsi = NULL;

	/* Setup callback functions. */
	hsi_set_read_cb(dev, cfhsi_omap_read_cb);
	hsi_set_write_cb(dev, cfhsi_omap_write_cb);
	hsi_set_port_event_cb(dev , cfhsi_omap_port_event_cb);

	/* Open OMAP HSI device. */
	res = hsi_open(dev);
	if (res) {
		printk(KERN_WARNING "hsi_proto_probe: failed to open HSI device: %d.\n", res);
		goto err_hsi_open;
	}

	/* CAIF HSI TX configuration. */
	tx_conf.mode = 1; /* HSI_MODE_STREAM. */
	tx_conf.flow = 0; /* Synchronized. */
	tx_conf.frame_size = 31;
	tx_conf.channels = 2;
	tx_conf.divisor = 1; /* divide by 2. */
	tx_conf.arb_mode = 0; /* HSI_ARBMODE_RR. */

	/* CAIF HSI RX configuration. */
	rx_conf.mode = 1; /* HSI_MODE_STREAM. */
	rx_conf.flow = 0; /* Synchronized. */
	rx_conf.frame_size = 31;
	rx_conf.channels = 2;
	rx_conf.divisor = 0;
	rx_conf.counters =
		(180 << HSI_COUNTERS_FT_OFFSET) |
		(7 << HSI_COUNTERS_TB_OFFSET) |
		(8 << HSI_COUNTERS_FB_OFFSET);

	/* Configure HSI TX. */
	res = hsi_ioctl(dev, HSI_IOCTL_SET_TX, &tx_conf);
	if (res) {
		printk(KERN_WARNING "hsi_proto_probe: failed to configure TX: %d.\n", res);
		goto err_hsi_ioctl;
	}

	/* Configure HSI RX. */
	res = hsi_ioctl(dev, HSI_IOCTL_SET_RX, &rx_conf);
	if (res) {
		printk(KERN_WARNING "hsi_proto_probe: failed to configure RX: %d.\n", res);
		goto err_hsi_ioctl;
	}

	hsi_close(dev);

	cfhsi = kzalloc(sizeof(struct cfhsi_omap), GFP_KERNEL);

	/* Assign OMAP HSI device to this CAIF HSI device. */
	cfhsi->hsi_dev = dev;
	cfhsi->dev.cfhsi_tx = cfhsi_tx;
	cfhsi->dev.cfhsi_rx = cfhsi_rx;
	cfhsi->dev.cfhsi_up = cfhsi_up;
	cfhsi->dev.cfhsi_down = cfhsi_down;
	cfhsi->dev.cfhsi_wake_up = cfhsi_wake_up;
	cfhsi->dev.cfhsi_wake_down = cfhsi_wake_down;
	cfhsi->dev.cfhsi_fifo_occupancy = cfhsi_fifo_occupancy;

	/* Initialize CAIF HSI platform device. */
	cfhsi->pdev.name = "cfhsi";
	cfhsi->pdev.dev.platform_data = &cfhsi->dev;
	cfhsi->pdev.dev.release = hsi_proto_release;
	/* Use channel number as id. Assuming only one HSI port. */
	cfhsi->pdev.id = cfhsi->hsi_dev->n_ch;
	/* Register platform device. */
	res = platform_device_register(&cfhsi->pdev);
	if (res) {
		dev_err(&dev->device, "%s: Failed to register dev: %d.\n",
			__func__, res);
		res = -ENODEV;
		goto err_reg_pdev;
	}

	/* Add HSI device to device list. */
	list_add_tail(&cfhsi->list, &cfhsi_dev_list);

	return res;

 err_reg_pdev:
	kfree(cfhsi);
 err_hsi_ioctl:
	hsi_close(dev);
 err_hsi_open:
	return res;
}

static int hsi_proto_remove(struct hsi_device *dev)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;
	struct list_head *n;

	list_for_each_safe(list_node, n, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Find the corresponding device. */
		if (cfhsi->hsi_dev == dev) {
			/* Remove from list. */
			list_del(list_node);
			/* Our HSI device is gone, unregister CAIF HSI device. */
			platform_device_del(&cfhsi->pdev);
			hsi_close(cfhsi->hsi_dev);
			/* Free memory. */
			kfree(cfhsi);
			return 0;
		}
	}
	return -ENODEV;
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
	.ch_mask[0] = (1 << 0),	/* TODO: Wait for second channel. | (1 << 1)*/
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
	int res;

	printk(KERN_INFO "%s.\n", __func__);

	/* Register protocol driver for HSI channel 0. */
	res =  hsi_register_driver(&cfhsi_omap_driver);
	if (res) {
		printk(KERN_ERR "%s: Failed to register CAIF HSI OMAP driver: %d.\n.",
			__func__, res);
	}

	return res;
}

static void __exit cfhsi_omap_exit(void)
{
	struct cfhsi_omap *cfhsi = NULL;
	struct list_head *list_node;
	struct list_head *n;
	int res;

	list_for_each_safe(list_node, n, &cfhsi_dev_list) {
		cfhsi = list_entry(list_node, struct cfhsi_omap, list);
		/* Remove from list. */
		list_del(list_node);

		/* Our HSI device is gone, unregister CAIF HSI device. */
		platform_device_del(&cfhsi->pdev);
		/*TODO: We should check if the device is open or not. */
		res = hsi_ioctl(cfhsi->hsi_dev, HSI_IOCTL_ACWAKE_DOWN, NULL);
		if (res) {
			printk(KERN_WARNING "%s: failed to set WAKE: %d\n",
					__func__, res);
		}
		hsi_close(cfhsi->hsi_dev);
		/* Free memory. */
		kfree(cfhsi);
	}

	/* Unregister driver. */
	hsi_unregister_driver(&cfhsi_omap_driver);
}

module_init(cfhsi_omap_init);
module_exit(cfhsi_omap_exit);
