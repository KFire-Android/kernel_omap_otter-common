/*
 * Copyright (c) 2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/page-flags.h>
#include <linux/pm.h>
#include <linux/sysdev.h>
#include <linux/vmalloc.h>
#include <linux/signal.h>
#ifdef CONFIG_ANDROID
#include <linux/device.h>
#endif
#include <linux/init.h>
#include <linux/bootmem.h>

#include "scx_protocol.h"
#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scxlnx_conn.h"
#include "scxlnx_comm.h"
#include "scxlnx_mshield.h"

#include "s_version.h"

#define SCX_SMC_PA_CTRL_START		0x1
#define SCX_SMC_PA_CTRL_STOP		0x2

#ifdef CONFIG_ANDROID
static struct class *tf_ctrl_class;
#endif

#define SCXLNX_DEVICE_CTRL_BASE_NAME "tf_ctrl"

struct SCX_SMC_PA_CTRL {
	u32 nPACommand;

	u32 nPASize;
	u8 *pPABuffer;

	u32 nConfSize;
	u8 *pConfBuffer;

	u32 nSDPBackingStoreAddr;
	u32 nSDPBkExtStoreAddr;
	u32 nDataAddr;

	u32 nClockTimerExpireMs;
};

static int SCXLNXCtrlDeviceIoctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long param)
{
	int nResult = S_SUCCESS;
	struct SCX_SMC_PA_CTRL paCtrl;
	u8 *pPABuffer = NULL;
	u8 *pPABufferRaw = NULL;
	u8 *pConfBuffer = NULL;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	dprintk(KERN_INFO "SCXLNXCtrlDeviceIoctl(%p, %u, %p)\n",
		file, cmd, (void *) param);

	if ((param & 0x3) != 0) {
		dprintk(KERN_ERR "SCXLNXCtrlDeviceIoctl(%p): "
			"ioctl command message pointer is not word "
			"aligned (%p)\n",
			file, (void *)param);

		nResult = -EFAULT;
		goto exit;
	}

	if (copy_from_user(&paCtrl, (struct SCX_SMC_PA_CTRL *)param,
			sizeof(struct SCX_SMC_PA_CTRL))) {
		dprintk(KERN_ERR "SCXLNXCtrlDeviceIoctl(%p): "
			"cannot access ioctl parameter (%p)\n",
			file, (void *)param);

		nResult = -EFAULT;
		goto exit;
	}

	switch (paCtrl.nPACommand) {
	case SCX_SMC_PA_CTRL_START:
		dprintk(KERN_INFO "SCXLNXCtrlDeviceIoctl(%p): "
			"Start the SMC PA (%d bytes) with conf (%d bytes)\n",
			file, paCtrl.nPASize, paCtrl.nConfSize);

		pPABufferRaw = (u8 *) internal_kmalloc(paCtrl.nPASize,
						GFP_KERNEL);
		if (pPABufferRaw == NULL) {
			dprintk(KERN_ERR "SCXLNXCtrlDeviceIoctl(%p): "
				"Out of memory for PA buffer\n", file);

			nResult = -ENOMEM;
			goto exit;
		}

		pPABuffer = (u8 *) internal_vmap_uncached(pPABufferRaw,
				paCtrl.nPASize);
		if (pPABuffer == NULL) {
			dprintk(KERN_ERR "SCXLNXCtrlDeviceIoctl(%p): "
				"Out of memory for PA buffer\n", file);

			internal_kfree(pPABufferRaw);

			nResult = -ENOMEM;
			goto exit;
		}

		if (copy_from_user(
				pPABuffer, paCtrl.pPABuffer, paCtrl.nPASize)) {
			dprintk(KERN_ERR "SCXLNXCtrlDeviceIoctl(%p): "
				"Cannot access PA buffer (%p)\n",
				file, (void *) paCtrl.pPABuffer);

			internal_vunmap(pPABuffer);
			internal_kfree(pPABufferRaw);

			nResult = -EFAULT;
			goto exit;
		}

		if (paCtrl.nConfSize > 0) {
			pConfBuffer = (u8 *) internal_kmalloc(
				paCtrl.nConfSize, GFP_KERNEL);
			if (pConfBuffer == NULL) {
				internal_vunmap(pPABuffer);
				internal_kfree(pPABufferRaw);

				nResult = -ENOMEM;
				goto exit;
			}

			if (copy_from_user(pConfBuffer,
					paCtrl.pConfBuffer, paCtrl.nConfSize)) {
				internal_vunmap(pPABuffer);
				internal_kfree(pPABufferRaw);
				internal_kfree(pConfBuffer);

				nResult = -EFAULT;
				goto exit;
			}
		}

#ifdef CONFIG_DYNAMIC_SDP_STORAGE_ALLOC
#define DATA_ADDRESS_OFFSET	0x20000
		if (pDevice->nSDPBackingStoreAddr != 0) {
			/*
			 * Override configuration to use dynamically allocated
			 * areas if SCXLNXCtrlDeviceEarlyInit was called.
			 */
			paCtrl.nSDPBackingStoreAddr =
				pDevice->nSDPBackingStoreAddr;
			paCtrl.nSDPBkExtStoreAddr =
				pDevice->nSDPBkExtStoreAddr;
			paCtrl.nDataAddr =
				paCtrl.nSDPBkExtStoreAddr + DATA_ADDRESS_OFFSET;

			dprintk(KERN_INFO "Overriding configuration: "
				"SDP addresses = (0x%08x, 0x%08x), "
				"SMC heap = 0x%08x\n",
				paCtrl.nSDPBackingStoreAddr,
				paCtrl.nSDPBkExtStoreAddr,
				paCtrl.nDataAddr);
		}
#endif

		nResult = SCXLNXCommStart(&pDevice->sm,
			paCtrl.nSDPBackingStoreAddr,
			paCtrl.nSDPBkExtStoreAddr,
			paCtrl.nDataAddr,
			pPABuffer, pPABufferRaw,
			paCtrl.nPASize,
			pConfBuffer, paCtrl.nConfSize);
		if (nResult)
			dprintk(KERN_ERR "SMC: start failed\n");
		else
			dprintk(KERN_INFO "SMC: started\n");

		internal_kfree(pConfBuffer);
		break;

	case SCX_SMC_PA_CTRL_STOP:
		dprintk(KERN_INFO "SCXLNXCtrlDeviceIoctl(%p): "
			"Stop the SMC PA\n", file);

		nResult = SCXLNXCommPowerManagement(&pDevice->sm,
			SCXLNX_POWER_OPERATION_SHUTDOWN);
		if (nResult)
			dprintk(KERN_WARNING "SMC: stop failed [0x%x]\n",
				nResult);
		else
			dprintk(KERN_INFO "SMC: stopped\n");
		break;

	default:
		nResult = -EOPNOTSUPP;
		break;
	}

exit:
	return nResult;
}

/*----------------------------------------------------------------------------*/

static int SCXLNXCtrlDeviceOpen(struct inode *inode, struct file *file)
{
	int nError;

	dprintk(KERN_INFO "SCXLNXCtrlDeviceOpen(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	/* Dummy lseek for non-seekable driver */
	nError = nonseekable_open(inode, file);
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXCtrlDeviceOpen(%p): "
			"nonseekable_open failed (error %d)!\n",
			file, nError);
		goto error;
	}

#ifndef CONFIG_ANDROID
	/*
	 * Check file flags. We only autthorize the O_RDWR access
	 */
	if (file->f_flags != O_RDWR) {
		dprintk(KERN_ERR "SCXLNXCtrlDeviceOpen(%p): "
			"Invalid access mode %u\n",
			file, file->f_flags);
		nError = -EACCES;
		goto error;
	}
#endif

	/*
	 * Successful completion.
	 */

	dprintk(KERN_INFO "SCXLNXCtrlDeviceOpen(%p): Success\n", file);
	return 0;

	/*
	 * Error handling.
	 */
error:
	dprintk(KERN_INFO "SCXLNXCtrlDeviceOpen(%p): Failure (error %d)\n",
		file, nError);
	return nError;
}

static const struct file_operations g_SCXLNXCtrlDeviceFileOps = {
	.owner = THIS_MODULE,
	.open = SCXLNXCtrlDeviceOpen,
	.ioctl = SCXLNXCtrlDeviceIoctl,
	.llseek = no_llseek,
};

int __init SCXLNXCtrlDeviceRegister(void)
{
	int nError;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	cdev_init(&pDevice->cdev_ctrl, &g_SCXLNXCtrlDeviceFileOps);
	pDevice->cdev_ctrl.owner = THIS_MODULE;

	nError = register_chrdev_region(pDevice->nDevNum + 1, 1,
		SCXLNX_DEVICE_CTRL_BASE_NAME);
	if (nError)
		return nError;

	nError = cdev_add(&pDevice->cdev_ctrl,
		pDevice->nDevNum + 1, 1);
	if (nError) {
		cdev_del(&(pDevice->cdev_ctrl));
		unregister_chrdev_region(pDevice->nDevNum + 1, 1);
		return nError;
	}

#ifdef CONFIG_ANDROID
	tf_ctrl_class = class_create(THIS_MODULE, SCXLNX_DEVICE_CTRL_BASE_NAME);
	device_create(tf_ctrl_class, NULL,
		pDevice->nDevNum + 1,
		NULL, SCXLNX_DEVICE_CTRL_BASE_NAME);
#endif

	return nError;
}

#ifdef CONFIG_DYNAMIC_SDP_STORAGE_ALLOC
int __init SCXLNXCtrlDeviceEarlyInit(void)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	/* Reserve memory areas for SDP operations */
	pDevice->nSDPBackingStoreAddr = (u32) __pa(alloc_bootmem(SZ_1M));
	pDevice->nSDPBkExtStoreAddr = (u32) __pa(alloc_bootmem(SZ_1M));

	printk(KERN_INFO "SMC: Allocated SDP Backing Storage (0x%x) and "
		"External Storage (0x%x) memory areas\n",
		pDevice->nSDPBackingStoreAddr,
		pDevice->nSDPBkExtStoreAddr);

	return 0;
}
#endif
