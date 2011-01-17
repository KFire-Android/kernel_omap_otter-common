/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
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

#include "scx_protocol.h"
#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scxlnx_conn.h"
#include "scxlnx_comm.h"
#ifdef CONFIG_TF_MSHIELD
#include <plat/cpu.h>
#include "scxlnx_mshield.h"
#endif

#include "s_version.h"

/*----------------------------------------------------------------------------
 * Forward Declarations
 *----------------------------------------------------------------------------*/

/*
 * Creates and registers the device to be managed by the specified driver.
 *
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
static int SCXLNXDeviceRegister(void);


/*
 * Implements the device Open callback.
 */
static int SCXLNXDeviceOpen(
		struct inode *inode,
		struct file *file);


/*
 * Implements the device Release callback.
 */
static int SCXLNXDeviceRelease(
		struct inode *inode,
		struct file *file);


/*
 * Implements the device ioctl callback.
 */
static long SCXLNXDeviceIoctl(
		struct file *file,
		unsigned int ioctl_num,
		unsigned long ioctl_param);


/*
 * Implements the device shutdown callback.
 */
static int SCXLNXDeviceShutdown(
		struct sys_device *sysdev);


/*
 * Implements the device suspend callback.
 */
static int SCXLNXDeviceSuspend(
		struct sys_device *sysdev,
		pm_message_t state);


/*
 * Implements the device resume callback.
 */
static int SCXLNXDeviceResume(
		struct sys_device *sysdev);


/*---------------------------------------------------------------------------
 * Module Parameters
 *---------------------------------------------------------------------------*/

/*
 * The device major number used to register a unique character device driver.
 * Let the default value be 122
 */
static int device_major_number = 122;

module_param(device_major_number, int, 0000);
MODULE_PARM_DESC(device_major_number,
	"The device major number used to register a unique character "
	"device driver");

#ifdef CONFIG_TF_TRUSTZONE
/**
 * The softint interrupt line used by the Secure World.
 */
static int soft_interrupt = -1;

module_param(soft_interrupt, int, 0000);
MODULE_PARM_DESC(soft_interrupt,
	"The softint interrupt line used by the Secure world");
#endif

#ifdef CONFIG_ANDROID
static struct class *tf_class;
#endif

/*----------------------------------------------------------------------------
 * Global Variables
 *----------------------------------------------------------------------------*/

/*
 * tf_driver character device definitions.
 * read and write methods are not defined
 * and will return an error if used by user space
 */
static const struct file_operations g_SCXLNXDeviceFileOps = {
	.owner = THIS_MODULE,
	.open = SCXLNXDeviceOpen,
	.release = SCXLNXDeviceRelease,
	.unlocked_ioctl = SCXLNXDeviceIoctl,
	.llseek = no_llseek,
};


static struct sysdev_class g_SCXLNXDeviceSysClass = {
	.name = SCXLNX_DEVICE_BASE_NAME,
	.shutdown = SCXLNXDeviceShutdown,
	.suspend = SCXLNXDeviceSuspend,
	.resume = SCXLNXDeviceResume,
};

/* The single device supported by this driver */
static struct SCXLNX_DEVICE g_SCXLNXDevice = {0, };

/*----------------------------------------------------------------------------
 * Implementations
 *----------------------------------------------------------------------------*/

struct SCXLNX_DEVICE *SCXLNXGetDevice(void)
{
	return &g_SCXLNXDevice;
}

/*
 * displays the driver stats
 */
static ssize_t kobject_show(struct kobject *pkobject,
	struct attribute *pattributes, char *buf)
{
	struct SCXLNX_DEVICE_STATS *pDeviceStats = &g_SCXLNXDevice.sDeviceStats;
	u32 nStatPagesAllocated;
	u32 nStatPagesLocked;
	u32 nStatMemoriesAllocated;

	nStatMemoriesAllocated =
		atomic_read(&(pDeviceStats->stat_memories_allocated));
	nStatPagesAllocated =
		atomic_read(&(pDeviceStats->stat_pages_allocated));
	nStatPagesLocked = atomic_read(&(pDeviceStats->stat_pages_locked));

	/*
	 * AFY: could we add the number of context switches (call to the SMI
	 * instruction)
	 */

	return snprintf(buf, PAGE_SIZE,
		"stat.memories.allocated: %d\n"
		"stat.pages.allocated:    %d\n"
		"stat.pages.locked:       %d\n",
		nStatMemoriesAllocated,
		nStatPagesAllocated,
		nStatPagesLocked);
}

static const struct sysfs_ops kobj_sysfs_operations = {
	.show = kobject_show,
};

/*----------------------------------------------------------------------------*/

/*
 * First routine called when the kernel module is loaded
 */
static int __init SCXLNXDeviceRegister(void)
{
	int nError;
	struct SCXLNX_DEVICE *pDevice = &g_SCXLNXDevice;
	struct SCXLNX_DEVICE_STATS *pDeviceStats = &pDevice->sDeviceStats;

	dprintk(KERN_INFO "SCXLNXDeviceRegister()\n");

#ifdef CONFIG_TF_MSHIELD
	/* No need to do anything on a GP device */
	switch (omap_type()) {
	case OMAP2_DEVICE_TYPE_GP:
		printk(KERN_INFO "SMC: Running on a GP device, SMC disabled\n");
		return 0;

	case OMAP2_DEVICE_TYPE_EMU:
	case OMAP2_DEVICE_TYPE_SEC:
		printk(KERN_INFO "SMC: Secure device detected, enabling SMC"
			" driver\n");
		break;

	default:
		return -EFAULT;
	}
#endif

	/*
	 * Initialize the device
	 */
	pDevice->nDevNum = MKDEV(device_major_number,
		SCXLNX_DEVICE_MINOR_NUMBER);
	cdev_init(&pDevice->cdev, &g_SCXLNXDeviceFileOps);
	pDevice->cdev.owner = THIS_MODULE;

	pDevice->sysdev.id = 0;
	pDevice->sysdev.cls = &g_SCXLNXDeviceSysClass;

	INIT_LIST_HEAD(&pDevice->conns);
	spin_lock_init(&pDevice->connsLock);

	/* register the sysfs object driver stats */
	pDeviceStats->kobj_type.sysfs_ops = &kobj_sysfs_operations;

	pDeviceStats->kobj_stat_attribute.name = "info";
	pDeviceStats->kobj_stat_attribute.mode = S_IRUGO;
	pDeviceStats->kobj_attribute_list[0] =
		&pDeviceStats->kobj_stat_attribute;

	pDeviceStats->kobj_type.default_attrs =
		pDeviceStats->kobj_attribute_list,
	kobject_init_and_add(&(pDeviceStats->kobj),
		 &(pDeviceStats->kobj_type), NULL, "%s",
		 SCXLNX_DEVICE_BASE_NAME);

	/*
	 * Register the system device.
	 */

	nError = sysdev_class_register(&g_SCXLNXDeviceSysClass);
	if (nError != 0) {
		printk(KERN_ERR "SCXLNXDeviceRegister():"
			" sysdev_class_register failed (error %d)!\n",
			nError);
		goto sysdev_class_register_failed;
	}

	nError = sysdev_register(&pDevice->sysdev);
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXDeviceRegister(): "
			"sysdev_register failed (error %d)!\n",
			nError);
		goto sysdev_register_failed;
	}

	/*
	 * Register the char device.
	 */
	printk(KERN_INFO "Registering char device %s (%u:%u)\n",
		SCXLNX_DEVICE_BASE_NAME,
		MAJOR(pDevice->nDevNum),
		MINOR(pDevice->nDevNum));
	nError = register_chrdev_region(pDevice->nDevNum, 1,
		SCXLNX_DEVICE_BASE_NAME);
	if (nError != 0) {
		printk(KERN_ERR "SCXLNXDeviceRegister():"
			" register_chrdev_region failed (error %d)!\n",
			nError);
		goto register_chrdev_region_failed;
	}

	nError = cdev_add(&pDevice->cdev, pDevice->nDevNum, 1);
	if (nError != 0) {
		printk(KERN_ERR "SCXLNXDeviceRegister(): "
			"cdev_add failed (error %d)!\n",
			nError);
		goto cdev_add_failed;
	}

	/*
	 * Initialize the communication with the Secure World.
	 */
#ifdef CONFIG_TF_TRUSTZONE
	pDevice->sm.nSoftIntIrq = soft_interrupt;
#endif
	nError = SCXLNXCommInit(&g_SCXLNXDevice.sm);
	if (nError != S_SUCCESS) {
		dprintk(KERN_ERR "SCXLNXDeviceRegister(): "
			"SCXLNXCommInit failed (error %d)!\n",
			nError);
		goto init_failed;
	}

#ifdef CONFIG_ANDROID
	tf_class = class_create(THIS_MODULE, SCXLNX_DEVICE_BASE_NAME);
	device_create(tf_class, NULL,
		pDevice->nDevNum,
		NULL, SCXLNX_DEVICE_BASE_NAME);
#endif

#ifdef CONFIG_TF_MSHIELD
	/*
	 * Initializes the /dev/xxx_ctrl device node used to start and stop the
	 * SMC PA
	 */
	nError = SCXLNXCtrlDeviceRegister();
	if (nError)
		goto init_failed;
#endif

#ifdef CONFIG_SMC_BENCH_SECURE_CYCLE
	runBogoMIPS();
	addressCacheProperty((unsigned long) &SCXLNXDeviceRegister);
#endif
	/*
	 * Successful completion.
	 */

	dprintk(KERN_INFO "SCXLNXDeviceRegister(): Success\n");
	return 0;

	/*
	 * Error: undo all operations in the reverse order
	 */
init_failed:
	cdev_del(&pDevice->cdev);
cdev_add_failed:
	unregister_chrdev_region(pDevice->nDevNum, 1);
register_chrdev_region_failed:
	sysdev_unregister(&(pDevice->sysdev));
sysdev_register_failed:
	sysdev_class_unregister(&g_SCXLNXDeviceSysClass);
sysdev_class_register_failed:
	kobject_del(&g_SCXLNXDevice.sDeviceStats.kobj);

	dprintk(KERN_INFO "SCXLNXDeviceRegister(): Failure (error %d)\n",
		nError);
	return nError;
}

/*----------------------------------------------------------------------------*/

static int SCXLNXDeviceOpen(struct inode *inode, struct file *file)
{
	int nError;
	struct SCXLNX_DEVICE *pDevice = &g_SCXLNXDevice;
	struct SCXLNX_CONNECTION *pConn = NULL;

	dprintk(KERN_INFO "SCXLNXDeviceOpen(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	/* Dummy lseek for non-seekable driver */
	nError = nonseekable_open(inode, file);
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXDeviceOpen(%p): "
			"nonseekable_open failed (error %d)!\n",
			file, nError);
		goto error;
	}

#ifndef CONFIG_ANDROID
	/*
	 * Check file flags. We only autthorize the O_RDWR access
	 */
	if (file->f_flags != O_RDWR) {
		dprintk(KERN_ERR "SCXLNXDeviceOpen(%p): "
			"Invalid access mode %u\n",
			file, file->f_flags);
		nError = -EACCES;
		goto error;
	}
#endif

	/*
	 * Open a new connection.
	 */

	nError = SCXLNXConnOpen(pDevice, file, &pConn);
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXDeviceOpen(%p): "
			"SCXLNXConnOpen failed (error %d)!\n",
			file, nError);
		goto error;
	}

	/*
	 * Attach the connection to the device.
	 */
	spin_lock(&(pDevice->connsLock));
	list_add(&(pConn->list), &(pDevice->conns));
	spin_unlock(&(pDevice->connsLock));

	file->private_data = pConn;

	/*
	 * Send the CreateDeviceContext command to the secure
	 */
	nError = SCXLNXConnCreateDeviceContext(pConn);
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXDeviceOpen(%p): "
			"SCXLNXConnCreateDeviceContext failed (error %d)!\n",
			file, nError);
		goto error1;
	}

	/*
	 * Successful completion.
	 */

	dprintk(KERN_INFO "SCXLNXDeviceOpen(%p): Success (pConn=%p)\n",
		file, pConn);
	return 0;

	/*
	 * Error handling.
	 */

error1:
	SCXLNXConnClose(pConn);
error:
	dprintk(KERN_INFO "SCXLNXDeviceOpen(%p): Failure (error %d)\n",
		file, nError);
	return nError;
}

/*----------------------------------------------------------------------------*/

static int SCXLNXDeviceRelease(struct inode *inode, struct file *file)
{
	struct SCXLNX_CONNECTION *pConn;

	dprintk(KERN_INFO "SCXLNXDeviceRelease(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	pConn = SCXLNXConnFromFile(file);
	spin_lock(&g_SCXLNXDevice.connsLock);
	list_del(&pConn->list);
	spin_unlock(&g_SCXLNXDevice.connsLock);
	SCXLNXConnClose(pConn);

	dprintk(KERN_INFO "SCXLNXDeviceRelease(%p): Success\n", file);
	return 0;
}

/*----------------------------------------------------------------------------*/

static long SCXLNXDeviceIoctl(struct file *file, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int nResult = S_SUCCESS;
	struct SCXLNX_CONNECTION *pConn;
	union SCX_COMMAND_MESSAGE sMessage;
	struct SCX_COMMAND_HEADER  sCommandHeader;
	union SCX_ANSWER_MESSAGE sAnswer;
	u32 nCommandSize;
	u32 nAnswerSize;
	void *pUserAnswer;

	dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p, %u, %p)\n",
		file, ioctl_num, (void *) ioctl_param);

	switch (ioctl_num) {
	case IOCTL_SCX_GET_VERSION:
		/* ioctl is asking for the driver interface version */
		nResult = SCX_DRIVER_INTERFACE_VERSION;
		goto exit;

	case IOCTL_SCX_EXCHANGE:
		/*
		 * ioctl is asking to perform a message exchange with the Secure
		 * Module
		 */

		/*
		 * Make a local copy of the data from the user application
		 * This routine checks the data is readable
		 *
		 * Get the header first.
		 */
		if (copy_from_user(&sCommandHeader,
				(struct SCX_COMMAND_HEADER *)ioctl_param,
				sizeof(struct SCX_COMMAND_HEADER))) {
			dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p): "
				"Cannot access ioctl parameter %p\n",
				file, (void *) ioctl_param);
			nResult = -EFAULT;
			goto exit;
		}

		/* size in words of u32 */
		nCommandSize = sCommandHeader.nMessageSize +
			sizeof(struct SCX_COMMAND_HEADER)/sizeof(u32);
		if (nCommandSize > sizeof(sMessage)/sizeof(u32)) {
			dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p): "
				"Buffer overflow: too many bytes to copy %d\n",
				file, nCommandSize);
			nResult = -EFAULT;
			goto exit;
		}

		if (copy_from_user(&sMessage,
				(union SCX_COMMAND_MESSAGE *)ioctl_param,
				nCommandSize * sizeof(u32))) {
			dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p): "
				"Cannot access ioctl parameter %p\n",
				file, (void *) ioctl_param);
			nResult = -EFAULT;
			goto exit;
		}

		pConn = SCXLNXConnFromFile(file);
		BUG_ON(pConn == NULL);

		/*
		 * The answer memory space address is in the nOperationID field
		 */
		pUserAnswer = (void *) sMessage.sHeader.nOperationID;

		atomic_inc(&(pConn->nPendingOpCounter));

		dprintk(KERN_WARNING "SCXLNXDeviceIoctl(%p): "
			"Sending message type  0x%08x\n",
			file, sMessage.sHeader.nMessageType);

		switch (sMessage.sHeader.nMessageType) {
		case SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
			nResult = SCXLNXConnOpenClientSession(pConn,
				&sMessage, &sAnswer);
			break;

		case SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
			nResult = SCXLNXConnCloseClientSession(pConn,
				&sMessage, &sAnswer);
			break;

		case SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
			nResult = SCXLNXConnRegisterSharedMemory(pConn,
				&sMessage, &sAnswer);
			break;

		case SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
			nResult = SCXLNXConnReleaseSharedMemory(pConn,
				&sMessage, &sAnswer);
			break;

		case SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
			nResult = SCXLNXConnInvokeClientCommand(pConn,
				&sMessage, &sAnswer);
			break;

		case SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
			nResult = SCXLNXConnCancelClientCommand(pConn,
				&sMessage, &sAnswer);
			break;

		default:
			dprintk(KERN_ERR "SCXLNXDeviceIoctlExchange(%p): "
				"Incorrect message type (0x%08x)!\n",
				pConn, sMessage.sHeader.nMessageType);
			nResult = -EOPNOTSUPP;
			break;
		}

		atomic_dec(&(pConn->nPendingOpCounter));

		if (nResult != 0) {
			dprintk(KERN_WARNING "SCXLNXDeviceIoctl(%p): "
				"Operation returning error code 0x%08x)!\n",
				file, nResult);
			goto exit;
		}

		/*
		 * Copy the answer back to the user space application.
		 * The driver does not check this field, only copy back to user
		 * space the data handed over by Secure World
		 */
		nAnswerSize = sAnswer.sHeader.nMessageSize +
			sizeof(struct SCX_ANSWER_HEADER)/sizeof(u32);
		if (copy_to_user(pUserAnswer,
				&sAnswer, nAnswerSize * sizeof(u32))) {
			dprintk(KERN_WARNING "SCXLNXDeviceIoctl(%p): "
				"Failed to copy back the full command "
				"answer to %p\n", file, pUserAnswer);
			nResult = -EFAULT;
			goto exit;
		}

		/* successful completion */
		dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p): Success\n", file);
		break;

	case  IOCTL_SCX_GET_DESCRIPTION: {
		/* ioctl asking for the version information buffer */
		struct SCX_VERSION_INFORMATION_BUFFER *pInfoBuffer;

		dprintk(KERN_INFO "IOCTL_SCX_GET_DESCRIPTION:(%p, %u, %p)\n",
			file, ioctl_num, (void *) ioctl_param);

		pInfoBuffer =
			((struct SCX_VERSION_INFORMATION_BUFFER *) ioctl_param);

		dprintk(KERN_INFO "IOCTL_SCX_GET_DESCRIPTION1: "
			"sDriverDescription=\"%64s\"\n", S_VERSION_STRING);

		if (copy_to_user(pInfoBuffer->sDriverDescription,
				S_VERSION_STRING,
				strlen(S_VERSION_STRING) + 1)) {
			dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p): "
				"Fail to copy back the driver description "
				"to  %p\n",
				file, pInfoBuffer->sDriverDescription);
			nResult = -EFAULT;
			goto exit;
		}

		dprintk(KERN_INFO "IOCTL_SCX_GET_DESCRIPTION2: "
			"sSecureWorldDescription=\"%64s\"\n",
			SCXLNXCommGetDescription(&g_SCXLNXDevice.sm));

		if (copy_to_user(pInfoBuffer->sSecureWorldDescription,
				SCXLNXCommGetDescription(&g_SCXLNXDevice.sm),
				SCX_DESCRIPTION_BUFFER_LENGTH)) {
			dprintk(KERN_WARNING "SCXLNXDeviceIoctl(%p): "
				"Failed to copy back the secure world "
				"description to %p\n",
				file, pInfoBuffer->sSecureWorldDescription);
			nResult = -EFAULT;
			goto exit;
		}
		break;
	}

	default:
		dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p): "
			"Unknown IOCTL code 0x%08x!\n",
			file, ioctl_num);
		nResult = -EOPNOTSUPP;
		goto exit;
	}

exit:
	return nResult;
}

/*----------------------------------------------------------------------------*/

static int SCXLNXDeviceShutdown(struct sys_device *sysdev)
{
	return SCXLNXCommPowerManagement(&g_SCXLNXDevice.sm,
		SCXLNX_POWER_OPERATION_SHUTDOWN);
}

/*----------------------------------------------------------------------------*/

static int SCXLNXDeviceSuspend(struct sys_device *sysdev, pm_message_t state)
{
	printk(KERN_INFO "SCXLNXDeviceSuspend: Enter\n");
	return SCXLNXCommPowerManagement(&g_SCXLNXDevice.sm,
		SCXLNX_POWER_OPERATION_HIBERNATE);
}


/*----------------------------------------------------------------------------*/

static int SCXLNXDeviceResume(struct sys_device *sysdev)
{
	return SCXLNXCommPowerManagement(&g_SCXLNXDevice.sm,
		SCXLNX_POWER_OPERATION_RESUME);
}


/*----------------------------------------------------------------------------*/

module_init(SCXLNXDeviceRegister);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trusted Logic S.A.");
