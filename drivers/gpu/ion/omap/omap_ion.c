/*
 * drivers/gpu/omap/omap_ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/omap_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "../ion_priv.h"
#include "omap_ion_priv.h"
#include <linux/module.h>
#include <linux/of.h>


struct ion_device *omap_ion_device;
EXPORT_SYMBOL(omap_ion_device);

static int num_heaps;
static struct ion_heap **heaps;
static struct ion_heap *tiler_heap;
static struct ion_heap *nonsecure_tiler_heap;

static struct ion_platform_data omap_ion_data = {
	.nr = 5,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_NONSECURE_TILER,
			.name = "nonsecure_tiler",
		},
		{
			.type = ION_HEAP_TYPE_SYSTEM,
			.id = OMAP_ION_HEAP_SYSTEM,
			.name = "system",
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER_RESERVATION,
			.id = OMAP_ION_HEAP_TILER_RESERVATION,
			.name = "tiler_reservation",
		},
	},
};


int omap_ion_tiler_alloc(struct ion_client *client,
			 struct omap_ion_tiler_alloc_data *data)
{
	return omap_tiler_alloc(tiler_heap, client, data);
}
EXPORT_SYMBOL(omap_ion_tiler_alloc);

int omap_ion_nonsecure_tiler_alloc(struct ion_client *client,
			 struct omap_ion_tiler_alloc_data *data)
{
	if (!nonsecure_tiler_heap)
		return -ENOMEM;
	return omap_tiler_alloc(nonsecure_tiler_heap, client, data);
}
EXPORT_SYMBOL(omap_ion_nonsecure_tiler_alloc);

static long omap_ion_ioctl(struct ion_client *client, unsigned int cmd,
		    unsigned long arg)
{
	switch (cmd) {
	case OMAP_ION_TILER_ALLOC:
	{
		struct omap_ion_tiler_alloc_data data;
		int ret;

		if (!tiler_heap) {
			pr_err("%s: Tiler heap requested but no tiler heap "
					"exists on this platform\n", __func__);
			return -EINVAL;
		}
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		ret = omap_ion_tiler_alloc(client, &data);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &data,
				 sizeof(data)))
			return -EFAULT;
		break;
	}
	case OMAP_ION_LOOKUP_SHARE_FD:
	{
		struct omap_ion_lookup_share_fd lookup_data;
		int ret;

		if (copy_from_user(&lookup_data,
				   (void __user *)arg,
				   sizeof(lookup_data)))
					return -EFAULT;
		ret = omap_ion_lookup_share_fd(lookup_data.alloc_fd,
						&lookup_data.num_fds,
						lookup_data.share_fds);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &lookup_data,
				 sizeof(lookup_data)))
			return -EFAULT;
		break;
	}
	default:
		pr_err("%s: Unknown custom ioctl\n", __func__);
		return -ENOTTY;
	}
	return 0;
}

static int omap_ion_probe(struct platform_device *pdev)
{
	int err;
	int i;
	struct device_node *node = pdev->dev.of_node;
	uint omap_ion_heap_secure_input_base = 0;
	uint omap_ion_heap_tiler_base = 0;
	uint omap_ion_heap_nonsecure_tiler_base = 0;
	uint omap_ion_heap_secure_input_size = 0;
	uint omap_ion_heap_tiler_size = 0;
	uint omap_ion_heap_nonsecure_tiler_size = 0;
	
	if (node) {
		of_property_read_u32(node, "ti,omap_ion_heap_secure_input_base",
				     &omap_ion_heap_secure_input_base);
		of_property_read_u32(node, "ti,omap_ion_heap_tiler_base",
				     &omap_ion_heap_tiler_base);
		of_property_read_u32(node, "ti,omap_ion_heap_nonsecure_tiler_base",
				     &omap_ion_heap_nonsecure_tiler_base);
		if (omap_ion_heap_secure_input_base == 0
			|| omap_ion_heap_tiler_base == 0
			|| omap_ion_heap_nonsecure_tiler_base == 0) {
			pr_err("%s: carveout memory address is null. please check dts file\n"
				"omap_ion_heap_secure_input_base = 0x%x\n"
				"omap_ion_heap_tiler_base = 0x%x\n"
				"omap_ion_heap_nonsecure_tiler_base = 0x%x\n"
				, __func__
				, omap_ion_heap_secure_input_base
				, omap_ion_heap_tiler_base
				, omap_ion_heap_tiler_base);
			return -EFAULT;
		}

		of_property_read_u32(node, "ti,omap_ion_heap_secure_input_size",
				     &omap_ion_heap_secure_input_size);
		of_property_read_u32(node, "ti,omap_ion_heap_tiler_size",
				     &omap_ion_heap_tiler_size);
		of_property_read_u32(node, "ti,omap_ion_heap_nonsecure_tiler_size",
				     &omap_ion_heap_nonsecure_tiler_size);
		if (omap_ion_heap_secure_input_size == 0
			|| omap_ion_heap_tiler_size == 0
			|| omap_ion_heap_nonsecure_tiler_size == 0) {
			pr_err("%s: carveout memory address is null. please check dts file\n"
				"omap_ion_heap_secure_input_size = 0x%x\n"
				"omap_ion_heap_tiler_size = 0x%x\n"
				"omap_ion_heap_nonsecure_tiler_size = 0x%x\n"
				, __func__
				, omap_ion_heap_secure_input_size
				, omap_ion_heap_tiler_size
				, omap_ion_heap_nonsecure_tiler_size);
			return -EINVAL;
		}

	} else {
		pr_err("%s: no matching device tree node\n", __func__);
		return -ENODEV;
	}

	omap_ion_device = ion_device_create(omap_ion_ioctl);
	if (IS_ERR_OR_NULL(omap_ion_device))
		return PTR_ERR(omap_ion_device);


	num_heaps = omap_ion_data.nr;

	heaps = kzalloc(sizeof(struct ion_heap *)*num_heaps, GFP_KERNEL);
	

	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &omap_ion_data.heaps[i];
		if (heap_data->type == (enum ion_heap_type)OMAP_ION_HEAP_TYPE_TILER) {
			if (heap_data->id == OMAP_ION_HEAP_NONSECURE_TILER) {
				heap_data->base = omap_ion_heap_nonsecure_tiler_base;
				heap_data->size = omap_ion_heap_nonsecure_tiler_size;
			} else {
				heap_data->base = omap_ion_heap_tiler_base;
				heap_data->size = omap_ion_heap_tiler_size;
			}
		} else if (heap_data->type ==
				ION_HEAP_TYPE_CARVEOUT && heap_data->id == OMAP_ION_HEAP_SECURE_INPUT) {
			heap_data->base = omap_ion_heap_secure_input_base;
			heap_data->size = omap_ion_heap_secure_input_size;
		}
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &omap_ion_data.heaps[i];
		if (heap_data->type == (enum ion_heap_type)OMAP_ION_HEAP_TYPE_TILER) {
			heaps[i] = omap_tiler_heap_create(heap_data);
			if (heap_data->id == OMAP_ION_HEAP_NONSECURE_TILER)
				nonsecure_tiler_heap = heaps[i];
			else
				tiler_heap = heaps[i];
		} else if (heap_data->type ==
				(enum ion_heap_type)OMAP_ION_HEAP_TYPE_TILER_RESERVATION) {
			heaps[i] = omap_tiler_heap_create(heap_data);
		} else {
			heap_data->size = omap_ion_heap_secure_input_size;
			heaps[i] = ion_heap_create(heap_data);
		}
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(omap_ion_device, heaps[i]);
		pr_info("%s: adding heap %s of type %d with %lx@%x\n",
			__func__, heap_data->name, heap_data->type,
			heap_data->base, heap_data->size);

	}

	platform_set_drvdata(pdev, omap_ion_device);
	return 0;
err:
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i]) {
			if (heaps[i]->type == (enum ion_heap_type)OMAP_ION_HEAP_TYPE_TILER)
				omap_tiler_heap_destroy(heaps[i]);
			else
				ion_heap_destroy(heaps[i]);
		}
	}
	kfree(heaps);
	return err;
}

static int omap_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		if (heaps[i]->type == (enum ion_heap_type)OMAP_ION_HEAP_TYPE_TILER)
			omap_tiler_heap_destroy(heaps[i]);
		else
			ion_heap_destroy(heaps[i]);
	kfree(heaps);
	return 0;
}

static void (*export_fd_to_ion_handles)(int fd,
		struct ion_client **client,
		struct ion_handle **handles,
		int *num_handles);

void omap_ion_register_pvr_export(void *pvr_export_fd)
{
	export_fd_to_ion_handles = pvr_export_fd;
}
EXPORT_SYMBOL(omap_ion_register_pvr_export);

int omap_ion_share_fd_to_buffers(int fd, struct ion_buffer **buffers,
		int *num_handles)
{
	struct ion_handle **handles;
	struct ion_client *client;
	int i = 0, ret = 0;
	int share_fd;

	handles = kzalloc(*num_handles * sizeof(struct ion_handle *),
			  GFP_KERNEL);
	if (!handles)
		return -ENOMEM;

	if (export_fd_to_ion_handles) {
		export_fd_to_ion_handles(fd,
				&client,
				handles,
				num_handles);
	} else {
		pr_err("%s: export_fd_to_ion_handles not initialized",
				__func__);
		ret = -EINVAL;
		goto exit;
	}

	for (i = 0; i < *num_handles; i++) {
		if (handles[i]) {
			share_fd = ion_share_dma_buf(client, handles[i]);
			buffers[i] = ion_handle_buffer(handles[i]);
		}
	}

exit:
	kfree(handles);
	return ret;
}
EXPORT_SYMBOL(omap_ion_share_fd_to_buffers);

int omap_ion_lookup_share_fd(int fd, int *num_handles, int *share_fds)
{
	struct ion_handle **handles;
	struct ion_client *client;
	int i = 0, ret = 0;

	handles = kzalloc(*num_handles * sizeof(struct ion_handle *),
			  GFP_KERNEL);
	if (!handles)
		return -ENOMEM;

	if (export_fd_to_ion_handles) {
		export_fd_to_ion_handles(fd, &client,
					 handles, num_handles);
	} else {
		pr_err("%s: export_fd_to_ion_handles not initialized",
		       __func__);
		ret = -EINVAL;
		goto exit;
	}

	for (i = 0; i < *num_handles; i++) {
		if (handles[i])
			share_fds[i] = ion_share_dma_buf(client, handles[i]);
	}

exit:
	kfree(handles);
	return ret;
}
EXPORT_SYMBOL(omap_ion_lookup_share_fd);

static const struct of_device_id omap_ion_of_match[] = {
	{.compatible = "ti,ion-omap", },
	{ },
};


static struct platform_driver ion_driver = {
	.probe = omap_ion_probe,
	.remove = omap_ion_remove,
	.driver = { .name = "ion-omap",
				.of_match_table = omap_ion_of_match
	}
};

static int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

module_init(ion_init);
module_exit(ion_exit);
