/*
 *Copyright (c)2006-2008 Trusted Logic S.A.
 *All Rights Reserved.
 *
 *This program is free software; you can redistribute it and/or
 *modify it under the terms of the GNU General Public License
 *version 2 as published by the Free Software Foundation.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program; if not, write to the Free Software
 *Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *MA 02111-1307 USA
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
#include <linux/device.h>
#include <linux/signal.h>
#ifdef SMC_EARLY_INIT
#include <linux/bootmem.h>
#endif
#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scxlnx_conn.h"
#include "scxlnx_sm_comm.h"

#include "s_version.h"

#ifdef CONFIG_ANDROID
static struct class *smc_class;
#endif	/*#ifdef CONFIG_ANDROID */

#ifdef SMODULE_SMC_OMAP3XXX_CRYPTOAPI_IMPL
#include "scxlnx_cryptoAPI.h"
#endif

/*--------------------------------------------------------------------------
 *Forward Declarations
 *-------------------------------------------------------------------------- */

/*
 *Creates and registers the device to be managed by the specified driver.
 *
 *Returns zero upon successful completion, or an appropriate error code upon
 *failure.
 */
static int SCXLNXDeviceRegister(void);

/*
 *Cleans up the registered device.
 */
static void SCXLNXDeviceCleanup(void);

/*
 *Retrieves a pointer to the monitor of the device referenced by the specified
 *inode.
 */
static inline SCXLNX_DEVICE_MONITOR *SCXLNXDeviceFromInode(struct inode *inode)
{
	return container_of(inode->i_cdev, SCXLNX_DEVICE_MONITOR, cdev);
}

/*
 *Retrieves a pointer to the monitor of the device referenced by the specified
 *sysdev.
 */
static inline SCXLNX_DEVICE_MONITOR *SCXLNXDeviceFromSysdev(struct sys_device
								 *sysdev)
{
	return container_of(sysdev, SCXLNX_DEVICE_MONITOR, sysdev);
}

/*
 *Implements the device Open callback.
 */
static int SCXLNXDeviceOpen(struct inode *inode, struct file *file);

/*
 *Implements the device Release callback.
 */
static int SCXLNXDeviceRelease(struct inode *inode, struct file *file);

/*
 *Implements the device ioctl callback.
 */
static long SCXLNXDeviceIoctl(struct file *file,
				unsigned int ioctl_num,
				unsigned long ioctl_param);

/*
 *Implements the device shutdown callback.
 */
static int SCXLNXDeviceShutdown(struct sys_device *sysdev);

/*
 *Implements the device mmap callback.
 */
static int SCXLNXDeviceMmap(struct file *file, struct vm_area_struct *vma);

/*--------------------------------------------------------------------------
 *Module Parameters
 *-------------------------------------------------------------------------- */

/*
 *The device major number used to register a unique character device driver.
 *Let the default value be 122
 */
static int device_major_number = 122;

module_param(device_major_number, int, 0000);
MODULE_PARM_DESC(device_major_number,
 "The device major number used to register a unique character device driver");

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
static int smc_suspend(struct sys_device *dev, pm_message_t state)
{
	powerPrintk("smc_suspend\n");

	/* As we enter in CORE OFF, the keys are going to be cleared.
	 * Reset the global key context.
	 * When the system leaves CORE OFF, this will force the driver to go
	 * through the secure world which will reconfigure the accelerator as
	 * public.
	 */
	g_SCXLNXDeviceMonitor.hAES1SecureKeyContext = 0;
	g_SCXLNXDeviceMonitor.hD3D2SecureKeyContext = 0;
	g_SCXLNXDeviceMonitor.bSHAM1IsPublic = false;

	return 0;
}

static int smc_resume(struct sys_device *dev)
{
	powerPrintk("smc_resume\n");

	/*
	 * When the system leaves CORE OFF, HWA have been put back in secure.
	 * Linux Crypto API needs them to be public.
	 */
	SCXLNXSMCommPowerManagement(&(g_SCXLNXDeviceMonitor.sm),
		SCXLNX_SM_POWER_OPERATION_RESUME);

	return 0;
}
#endif

/*--------------------------------------------------------------------------
 *Global Variables
 *-------------------------------------------------------------------------- */

/*
 *smodule character device definitions.
 *read and write methods are not defined
 *and will return an error if used by user space
 */
static struct file_operations g_SCXLNXDeviceFileOps = {
	.owner = THIS_MODULE,
	.open = SCXLNXDeviceOpen,
	.release = SCXLNXDeviceRelease,
	.unlocked_ioctl = SCXLNXDeviceIoctl,
	.mmap = SCXLNXDeviceMmap,
	.llseek = no_llseek,
};

static struct sysdev_class g_SCXLNXDeviceSysClass = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
	.name = SCXLNX_DEVICE_BASE_NAME,
#else
	set_kset_name(SCXLNX_DEVICE_BASE_NAME),
#endif
	.shutdown = SCXLNXDeviceShutdown,
#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
	.suspend = smc_suspend,
	.resume = smc_resume,
#endif
};

SCXLNX_DEVICE_MONITOR g_SCXLNXDeviceMonitor;

/*-------------------------------------------------------------------------
 *Implementations
 *------------------------------------------------------------------------- */

/*
 *deisplays the driver stats
 */
ssize_t kobject_show(struct kobject *pkobject, struct attribute *pattributes,
			  char *buf)
{
	SCXLNX_DEVICE_STATS *pDeviceStats = &g_SCXLNXDeviceMonitor.
							sDeviceStats;
	u32 nStatPagesAllocated;
	u32 nStatPagesLocked;
	u32 nStatMemoriesAllocated;

	nStatMemoriesAllocated =
		 atomic_read(&(pDeviceStats->stat_memories_allocated));
	nStatPagesAllocated =
		 atomic_read(&(pDeviceStats->stat_pages_allocated));
	nStatPagesLocked = atomic_read(&(pDeviceStats->stat_pages_locked));

	return snprintf(buf,
			PAGE_SIZE,
			"stat.memories.allocated: %d\n"
			"stat.pages.allocated:	 %d\n"
			"stat.pages.locked:		 %d\n",
			nStatMemoriesAllocated,
			nStatPagesAllocated, nStatPagesLocked);
}

/*
 *Attaches the specified connection to the specified device.
 */
void SCXLNXDeviceAttachConn(SCXLNX_DEVICE_MONITOR *pDevice,
				 SCXLNX_CONN_MONITOR *pConn)
{
	spin_lock(&(pDevice->connsLock));
	list_add(&(pConn->list), &(pDevice->conns));
	spin_unlock(&(pDevice->connsLock));
}

/*-------------------------------------------------------------------------- */

/*
 *Detaches the specified connection from the specified device.
 *
 *This function does nothing if pDevice or pConn is set to NULL, or if the
 *specified connection is not attached to the specified device.
 */
void SCXLNXDeviceDetachConn(SCXLNX_DEVICE_MONITOR *pDevice,
				 SCXLNX_CONN_MONITOR *pConn)
{
	if (pDevice == NULL || pConn == NULL || pConn->pDevice != pDevice)
		return;

	spin_lock(&(pDevice->connsLock));
	list_del(&(pConn->list));
	spin_unlock(&(pDevice->connsLock));
}

/*---------------------------------------------------------------------------
 *Driver Interface Implementations
 *-------------------------------------------------------------------------- */
#ifdef SMC_EARLY_INIT
void __init tf_allocate_workspace(void)
{
	memset(&g_SCXLNXDeviceMonitor, 0, sizeof(g_SCXLNXDeviceMonitor));

	g_SCXLNXDeviceMonitor.nSDPBkExtStoreAddr = __pa(
		__alloc_bootmem(SZ_2M, SZ_1M, __pa(MAX_DMA_ADDRESS)));
}
#endif /* SMC_EARLY_INIT */

/*
 *First routine called when the kernel module is loaded
 */
static int SCXLNXDeviceRegister(void)
{
	int nError;
	SCXLNX_DEVICE_MONITOR *pDevice = &g_SCXLNXDeviceMonitor;
	SCXLNX_DEVICE_STATS *pDeviceStats = &pDevice->sDeviceStats;

	dprintk(KERN_INFO "SCXLNXDeviceRegister\n");

	/*
	 *Initialize the device monitor.
	 */
#ifndef SMC_EARLY_INIT
	memset(pDevice->sDriverDescription, 0,
			 sizeof(pDevice->sDriverDescription));
#endif
	memcpy(pDevice->sDriverDescription, S_VERSION_STRING,
			 sizeof(S_VERSION_STRING) - 1);

	pDevice->nDevNum =
		 MKDEV(device_major_number, SCXLNX_DEVICE_MINOR_NUMBER);
	cdev_init(&(pDevice->cdev), &g_SCXLNXDeviceFileOps);
	pDevice->cdev.owner = THIS_MODULE;
	pDevice->cdev.ops = &g_SCXLNXDeviceFileOps;

	pDevice->sysdev.id = 0;
	pDevice->sysdev.cls = &g_SCXLNXDeviceSysClass;

	INIT_LIST_HEAD(&(pDevice->conns));
	spin_lock_init(&(pDevice->connsLock));

	/*register the sysfs object driver stats */
	pDeviceStats->kobj_sysfs_operations.show = kobject_show;
	pDeviceStats->kobj_type.sysfs_ops =
		 &pDeviceStats->kobj_sysfs_operations;

	pDeviceStats->kobj_stat_attribute.name = "info";
	pDeviceStats->kobj_stat_attribute.mode = S_IRUGO;
	pDeviceStats->kobj_attribute_list[0] =
		 &pDeviceStats->kobj_stat_attribute;

	pDeviceStats->kobj_type.default_attrs =
		 pDeviceStats->kobj_attribute_list,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
		 nError =
		 kobject_init_and_add(&(pDeviceStats->kobj),
				 &(pDeviceStats->kobj_type), NULL, "%s",
				 SCXLNX_DEVICE_BASE_NAME);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceRegister: \
			kobject_register failed [%d] !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_SYSFS_REGISTERED, &(pDevice->nFlags));
#else
		 kobject_set_name(&pDeviceStats->kobj,
				SCXLNX_DEVICE_BASE_NAME);
	pDeviceStats->kobj.ktype = &pDeviceStats->kobj_type;
	nError = kobject_register(&pDeviceStats->kobj);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceRegister: \
			kobject_register failed [%d] !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_SYSFS_REGISTERED, &(pDevice->nFlags));
#endif

	/*
	 *Register the system device.
	 */

	nError = sysdev_class_register(&g_SCXLNXDeviceSysClass);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SMC: DeviceRegister: \
			sysdev_class_register failed (error %d) !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_FLAG_SYSDEV_CLASS_REGISTERED,
		&(pDevice->nFlags));

	nError = sysdev_register(&(pDevice->sysdev));
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceRegister: \
			sysdev_register failed (error %d) !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_FLAG_SYSDEV_REGISTERED, &(pDevice->nFlags));

	/*
	 *Register the char device.
	 */

	nError =
		 register_chrdev_region(pDevice->nDevNum, 1,
					SCXLNX_DEVICE_BASE_NAME);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SMC: DeviceRegister: \
			register_chrdev_region failed (error %d) !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_FLAG_CDEV_REGISTERED, &(pDevice->nFlags));

	/*cdev_add must be called last */
	nError = cdev_add(&(pDevice->cdev), pDevice->nDevNum, 1);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SMC: DeviceRegister: cdev_add failed (error %d) !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_FLAG_CDEV_ADDED, &(pDevice->nFlags));

	/*
	 *Initialize the communication with the PA.
	 */
	nError = SCXLNXSMCommInit(&(pDevice->sm));
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceRegister: \
			SCXLNXSMCommInit failed [%d] !\n",
			nError);
		goto error;
	}
	set_bit(SCXLNX_DEVICE_FLAG_CDEV_INITIALIZED, &(pDevice->nFlags));

#ifdef CONFIG_ANDROID
	smc_class = class_create(THIS_MODULE, "smc_driver");
	device_create(smc_class, NULL,
				MKDEV(device_major_number,
					SCXLNX_DEVICE_MINOR_NUMBER),
				NULL, SCXLNX_DEVICE_BASE_NAME);
#endif /*#ifdef CONFIG_ANDROID */

#ifdef SMODULE_SMC_OMAP3XXX_CRYPTOAPI_IMPL
	nError = scxlnxCryptoAPI_init(&(pDevice->sm));
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceRegister: \
			scxlnxCryptoAPI_init failed [%d] !\n",
			nError);
		goto error;
	}
#endif		/*SMODULE_SMC_OMAP3XXX_CRYPTOAPI_IMPL */
	/*
	 *Successful completion.
	 */
	dprintk(KERN_INFO "SMC: Driver registered (%s, %u:%u)/ %s\n",
		SCXLNX_DEVICE_BASE_NAME,
		MAJOR(pDevice->nDevNum),
		MINOR(pDevice->nDevNum), S_VERSION_STRING);
#ifndef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
	dprintk(KERN_INFO "SMC: Version without power management !\n");
#endif

	return 0;

	/*
	 *Error handling.
	 */
 error:
	SCXLNXDeviceCleanup();
	dprintk(KERN_ERR "SCXLNXDeviceRegister: Failure (error %d)\n", nError);
	return nError;
}

/*-------------------------------------------------------------------------- */

/*
 *Routine called when the SCXLNXDeviceRegister()operation fails
 *The routine frees resources in the opposite order
 *of their creation.
 */
static void SCXLNXDeviceCleanup(void)
{
	SCXLNX_DEVICE_MONITOR *pDevice = &g_SCXLNXDeviceMonitor;

	dprintk(KERN_INFO "SCXLNXDeviceCleanup\n");

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
	/*To do :
	 *     . Unregister to the power management framework
	 *     . Release the VDD2 resource
	 */
#endif				/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

	if (test_bit(SCXLNX_DEVICE_FLAG_CDEV_ADDED, &(pDevice->nFlags)) != 0)
		cdev_del(&(pDevice->cdev));

	if (test_bit(SCXLNX_DEVICE_FLAG_CDEV_REGISTERED,
						&(pDevice->nFlags)) != 0) {
		unregister_chrdev_region(pDevice->nDevNum, 1);
	}

	if (test_bit(SCXLNX_DEVICE_FLAG_SYSDEV_REGISTERED,
						&(pDevice->nFlags)) != 0) {
		sysdev_unregister(&(pDevice->sysdev));
	}

	if (test_bit(SCXLNX_DEVICE_FLAG_SYSDEV_CLASS_REGISTERED,
						&(pDevice->nFlags)) != 0) {
		sysdev_class_unregister(&g_SCXLNXDeviceSysClass);
	}

	if (test_bit(SCXLNX_DEVICE_SYSFS_REGISTERED,
						&(pDevice->nFlags)) != 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
		kobject_del(&g_SCXLNXDeviceMonitor.sDeviceStats.kobj);
#else
		kobject_unregister(&g_SCXLNXDeviceMonitor.sDeviceStats.kobj);
#endif
	}
}

/*-------------------------------------------------------------------------- */

static int SCXLNXDeviceOpen(struct inode *inode, struct file *file)
{
	int nError;
	SCXLNX_DEVICE_MONITOR *pDevice = NULL;
	SCXLNX_CONN_MONITOR *pConn = NULL;

	dprintk(KERN_INFO "SCXLNXDeviceOpen(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	/*Dummy lseek for non-seekable driver */
	nError = nonseekable_open(inode, file);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceOpen(%p) : \
			nonseekable_open failed (error %d) !\n",
			file, nError);
		goto error;
	}

	file->private_data = NULL;

#ifndef CONFIG_ANDROID
	/*
	 *Check file flags. We only autthorize the O_RDWR access
	 */
	if (file->f_flags != O_RDWR) {
		dprintk(KERN_ERR
			"SCXLNXDeviceOpen(%p) : \
			Invalid access mode %u\n", file,
			file->f_flags);
		nError = -EACCES;
		goto error;
	}
#endif  /*#ifndef CONFIG_ANDROID */

	/*
	 *Retrieve the device identified by the inode.
	 */

	pDevice = SCXLNXDeviceFromInode(inode);

	/*
	 *Open a new connection.
	 */

	nError = SCXLNXConnOpen(pDevice, file, &pConn);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceOpen(%p) : \
			SCXLNXConnOpen failed (error %d) !\n",
			file, nError);
		goto error;
	}

	/*
	 *Attach the connection to the device.
	 */
	SCXLNXDeviceAttachConn(pDevice, pConn);

	file->private_data = pConn;

	/*
	 *Successful completion.
	 */

	dprintk(KERN_INFO "SCXLNXDeviceOpen(%p) : Success (pConn=%p)\n",
		file, pConn);
	return 0;

	/*
	 *Error handling.
	 */

 error:
	dprintk(KERN_INFO "SCXLNXDeviceOpen(%p) : Failure (error %d)\n",
		file, nError);
	return nError;
}

/*-------------------------------------------------------------------------- */

static int SCXLNXDeviceRelease(struct inode *inode, struct file *file)
{

	SCXLNX_CONN_MONITOR *pConn;

	dprintk(KERN_INFO "SCXLNXDeviceRelease(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	pConn = SCXLNXConnFromFile(file);
	SCXLNXDeviceDetachConn(pConn->pDevice, pConn);
	SCXLNXConnClose(pConn);

	dprintk(KERN_INFO "SCXLNXDeviceRelease(%p) : Success\n", file);

	return 0;
}

/*-------------------------------------------------------------------------- */

static long SCXLNXDeviceIoctl(struct file *file,
				unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	int nResult;
	SCXLNX_CONN_MONITOR *pConn;
	SCX_COMMAND_MESSAGE sMessage;
	SCX_ANSWER_MESSAGE sAnswer;
	void *pUserAnswer;

	dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p, %u, %p)\n",
		file, ioctl_num, (void *)ioctl_param);

	switch (ioctl_num) {

	case IOCTL_SCX_GET_VERSION:
		/*ioctl is asking for the driver version */
		nResult = SCX_DRIVER_INTERFACE_VERSION;
		dprintk(KERN_INFO
			"SCXLNXDeviceIoctl(%p, %p) : GET_VERSION=0x%08x\n",
			file, (void *)ioctl_param, nResult);
		break;

	case IOCTL_SCX_SMC_PA_CTRL:
		/*
		 *ioctl is asking to load a PA in the SE
		 */
		dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p, %p) : SMC_PA_CTRL\n",
			file, (void *)ioctl_param);

		pConn = SCXLNXConnFromFile(file);
		BUG_ON(pConn == NULL);

		{
			SCX_SMC_PA_CTRL paCtrl;
			u8 *pPABuffer = NULL;
			u8 *pPABufferRaw = NULL;
			u8 *pConfBuffer = NULL;

			/*verify the parameter pointer is word aligned */
			if ((ioctl_param & 0x3) != 0) {
				dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p) : \
					ioctl command message pointer is not \
					word aligned (%p)\n",
					file, (void *)ioctl_param);
				nResult = -EFAULT;
				goto exit;
			}

			/*
			 *Make a local copy of the data from the user
			 *application. This routine checks the data is readable
			 */
			if (copy_from_user
				 (&paCtrl, (SCX_SMC_PA_CTRL *) ioctl_param,
				  sizeof(SCX_SMC_PA_CTRL))) {
				dprintk(KERN_ERR "SCXLNXDeviceIoctl(%p) : \
					Cannot access ioctl parameter %p\n",
					file, (void *)ioctl_param);
				nResult = -EFAULT;
				goto exit;
			}

			switch (paCtrl.nPACommand) {
			case SCX_SMC_PA_CTRL_START:
				dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p) : \
					Start the SMC PA (%d bytes)with Conf \
					(%d bytes)\n",
					file, paCtrl.nPASize,
					paCtrl.nConfSize);

				/*map the address */
				pPABuffer =
					 (u8 *) internal_kmalloc_vmap((void **)
							 &pPABufferRaw,
							 paCtrl.nPASize,
							 GFP_KERNEL);
				if (pPABuffer == NULL) {
					dprintk(KERN_ERR
						"SCXLNXDeviceIoctl(%p) : \
						Out of memory for PA buffer \
						(%d bytes)\n",
						file, paCtrl.nPASize);
					nResult = -ENOMEM;
					goto exit;
				}

				if (copy_from_user
					 (pPABuffer, paCtrl.pPABuffer,
					  paCtrl.nPASize)) {
					internal_vunmap(pPABuffer);
					internal_kfree(pPABufferRaw);
					pPABufferRaw = NULL;
					pPABuffer = NULL;
					dprintk(KERN_ERR
						"SCXLNXDeviceIoctl(%p) : \
						Cannot access PA buffer %p\n",
						file,
						(void *)paCtrl.pPABuffer);
					nResult = -EFAULT;
					goto exit;
				}

				if (paCtrl.nConfSize != 0) {
					pConfBuffer =
						 (u8 *) internal_kmalloc(
							paCtrl.nConfSize,
							GFP_KERNEL);
					if (pConfBuffer == NULL) {
						internal_vunmap(pPABuffer);
						internal_kfree(pPABufferRaw);
						dprintk(KERN_ERR
						"SCXLNXDeviceIoctl(%p) : \
						Out of memory for Configuation\
						buffer (%d bytes)\n",
						file, paCtrl.nConfSize);
						nResult = -ENOMEM;
						goto exit;
					}

					if (copy_from_user(pConfBuffer,
						 paCtrl.pConfBuffer,
						 paCtrl.nConfSize)) {
						internal_vunmap(pPABuffer);
						internal_kfree(pPABufferRaw);
						pPABufferRaw = NULL;
						internal_kfree(pConfBuffer);
						pPABuffer = NULL;
						dprintk(KERN_ERR
						"SCXLNXDeviceIoctl(%p) : \
						Cannot access Configuration \
						buffer %p\n",
							file,
							(void *)paCtrl.
							pConfBuffer);
						nResult = -EFAULT;
						goto exit;
					}
				}
#ifdef SMC_EARLY_INIT
				/* Override config file */
				if (g_SCXLNXDeviceMonitor.nSDPBkExtStoreAddr) {
					paCtrl.nSDPBkExtStoreAddr =
						g_SCXLNXDeviceMonitor.
							nSDPBkExtStoreAddr;
					paCtrl.nSDPBackingStoreAddr =
						g_SCXLNXDeviceMonitor.
							nSDPBkExtStoreAddr +
							0x40000;
				}
#endif /* SMC_EARLY_INIT */
				/*
				 *Implementation note:
				 * The PA buffer will be released through a
				 * RPC call at the beginning of the PA entry
				 * in the SE.
				 * In case of error, the SCXLNXPACommStart
				 * will own the PA buffer, and will be
				 * responsible for releasing the buffer.
				 */

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
				g_SCXLNXDeviceMonitor.sm.nSDPBkExtStoreAddr =
					 paCtrl.nSDPBkExtStoreAddr;
				(pConn->pDevice->sm).nInactivityTimerExpireMs =
					 paCtrl.nClockTimerExpireMs;
#endif		/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

				nResult =
					 SCXLNXSMCommStart(&(pConn->
						pDevice->sm),
						paCtrl.
						nSDPBackingStoreAddr,
						paCtrl.nSDPBkExtStoreAddr,
						pPABuffer, pPABufferRaw,
						paCtrl.nPASize,
						pConfBuffer,
						paCtrl.nConfSize);
				if (nResult != 0) {
					dprintk(KERN_ERR
						"SMC: start failed [%d]\n",
						nResult);
				} else {
					dprintk(KERN_INFO "SMC: started\n");
				}
				internal_kfree(pConfBuffer);
				break;

			case SCX_SMC_PA_CTRL_STOP:
				dprintk(KERN_INFO
					"SCXLNXDeviceIoctl(%p) : \
					Stop the SMC PA\n",
					file);
				nResult =
					 SCXLNXSMCommPowerManagement(&
						(pConn->
						 pDevice->sm),
					SCXLNX_SM_POWER_OPERATION_SHUTDOWN);

				if (nResult != 0) {
					dprintk(KERN_WARNING
						"SMC: stop failed [%d]\n",
						nResult);
				} else {
					dprintk(KERN_INFO "SMC: stopped\n");
				}
				break;

			default:
				dprintk(KERN_ERR
					"SCXLNXDeviceIoctl(%p) : \
					Incorrect PA control type (0x%08x)!\n",
					file, paCtrl.nPACommand);
				nResult = -EOPNOTSUPP;
				break;
			}

			if (nResult != 0) {
				dprintk(KERN_ERR
					"SCXLNXDeviceIoctl(%p) : \
					SMC_PA_CTRL error [0x%x] !\n",
					file, nResult);
				goto exit;
			}
		}

		/*successful completion */
		dprintk(KERN_INFO
			"SCXLNXDeviceIoctl(%p) : SMC_PA_CTRL success\n", file);
		break;

	case IOCTL_SCX_EXCHANGE:
		/*
		 *ioctl is asking to start a message exchange with
		 *the Secure Module
		 */
		dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p, %p) : EXCHANGE\n",
			file, (void *)ioctl_param);

		/*verify the command message pointer is word aligned */
		if ((ioctl_param & 0x3) != 0) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : \
				ioctl command message pointer is not word \
				aligned (%p)\n",
				file, (void *)ioctl_param);
			nResult = -EFAULT;
			goto exit;
		}

		/*
		 *Make a local copy of the data from the user application
		 *This routine checks the data is readable
		 */
		if (copy_from_user
			 (&sMessage, (SCX_COMMAND_MESSAGE *) ioctl_param,
			  sizeof(SCX_COMMAND_MESSAGE))) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : \
				Cannot access ioctl parameter %p\n",
				file, (void *)ioctl_param);
			nResult = -EFAULT;
			goto exit;
		}

		pConn = SCXLNXConnFromFile(file);
		BUG_ON(pConn == NULL);

		/*
		 * The answer memory space address is in
		 * the nOperationID field
		 */
		pUserAnswer = (void *)sMessage.nOperationID;

		/*verify the answer message pointer is word aligned */
		if (((unsigned long)pUserAnswer & 0x3) != 0) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : \
				ioctl answer message pointer is not word \
				aligned (%p)\n",
				file, pUserAnswer);
			nResult = -EFAULT;
			goto exit;
		}

		/*
		 * Make sure that we can access the Answer memory field
		 * of the message
		 */
		if (!access_ok(VERIFY_WRITE, pUserAnswer,
				sizeof(SCX_ANSWER_MESSAGE))) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : \
				Write access not granted for ioctl answer \
				parameter %p\n",
				file, pUserAnswer);
			nResult = -EFAULT;
			goto exit;
		}

		if ((test_bit
			  (SCXLNX_SM_COMM_FLAG_POLLING_THREAD_STARTED,
				&(pConn->pDevice->sm.nFlags))) == 0) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : SM not started\n",
				file);
			nResult = -EFAULT;
			goto exit;
		}

		atomic_inc(&(pConn->nPendingOpCounter));
		switch (sMessage.nMessageType) {
		case SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
			nResult =
				 SCXLNXConnCreateDeviceContext(pConn,
							&sMessage,
							&sAnswer);
			break;
		case SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
			nResult =
				 SCXLNXConnOpenClientSession(pConn, &sMessage,
							&sAnswer);
			break;
		case SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
			nResult =
				 SCXLNXConnCloseClientSession(pConn, &sMessage,
							&sAnswer);
			break;
		case SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
			nResult =
				 SCXLNXConnRegisterSharedMemory(pConn,
							&sMessage,
							&sAnswer);
			break;
		case SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
			nResult =
				 SCXLNXConnReleaseSharedMemory(pConn,
							&sMessage,
							&sAnswer);
			break;
		case SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
			nResult =
				 SCXLNXConnInvokeClientCommand(pConn,
							&sMessage,
							&sAnswer);
			break;
		case SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
			nResult =
				 SCXLNXConnCancelClientCommand(pConn,
							&sMessage,
							&sAnswer);
			break;
		default:
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctlExchange(%p) : \
				Incorrect message type (0x%08x) !\n",
				pConn, sMessage.nMessageType);
			nResult = -EOPNOTSUPP;
			break;
		}

		atomic_dec(&(pConn->nPendingOpCounter));

		if (nResult != 0) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : \
				EXCHANGE error [0x%x] !\n",
				file, nResult);
			goto exit;
		}

		/*
		 *Copy the answer back to the user space application.
		 *The driver does not check this field,
		 *only copy back to user space the data handed over by SM
		 */
		if (copy_to_user(pUserAnswer, &sAnswer,
				sizeof(SCX_ANSWER_MESSAGE))) {
			dprintk(KERN_ERR
				"SCXLNXDeviceIoctl(%p) : \
				Failed to copy back the full \
				command answer to %p\n",
				file, pUserAnswer);
			nResult = -EFAULT;
			goto exit;
		}

		/*successful completion */
		dprintk(KERN_INFO "SCXLNXDeviceIoctl(%p) : EXCHANGE success\n",
			file);

		break;

	default:
		dprintk(KERN_ERR
			"SCXLNXDeviceIoctl(%p) : \
			Unknown IOCTL code [0x%08x] !\n",
			file, ioctl_num);
		nResult = -EOPNOTSUPP;
		goto exit;
	}

 exit:
	return nResult;
}

/*-------------------------------------------------------------------------- */

static int SCXLNXDeviceShutdown(struct sys_device *sysdev)
{
	return 0;
}

/*-------------------------------------------------------------------------- */

static void static_SCXLNXVmaOpen(struct vm_area_struct *vma)
{
	dprintk(KERN_INFO
		"static_SCXLNXVmaOpen(start=0x%lx, end=0x%lx, pgoff=0x%lx)\n",
		vma->vm_start, vma->vm_end, vma->vm_pgoff);
}

static void static_SCXLNXVmaClose(struct vm_area_struct *vma)
{
	dprintk(KERN_INFO "static_SCXLNXVmaClose\n");
}

/*-------------------------------------------------------------------------- */

static struct vm_operations_struct g_sSCXLNXMmops = {
	.open = static_SCXLNXVmaOpen,
	.close = static_SCXLNXVmaClose,
};

static int SCXLNXDeviceMmap(struct file *file, struct vm_area_struct *vma)
{
	SCXLNX_CONN_MONITOR *pConn;
	u32 nLength;
	u8 *pVirtAddr;
	u32 nPhysAddr;
	int nError;
	pgprot_t prot;

	dprintk(KERN_INFO
		"SCXLNXDeviceMmap(%p) : start=0x%lx, end=0x%lx, pgoff=0x%lx\n",
		file, vma->vm_start, vma->vm_end, vma->vm_pgoff);

	pConn = SCXLNXConnFromFile(file);
	BUG_ON(pConn == NULL);

	nLength = vma->vm_end - vma->vm_start;
	pVirtAddr = (u8 *) (vma->vm_pgoff << PAGE_SHIFT);

	dprintk(KERN_INFO "SCXLNXDeviceMmap(%p) : \
		Mapping VA=0x%x, Length=%u\n",
		pConn, (u32) pVirtAddr, nLength);

	/*Check buffer and length:
	 *do not allow larger mapping than the number of pages allocated
	 */
	nPhysAddr =
		SCXLNXConnCheckSharedMem(&pConn->sSharedMemoryMonitor,
					pVirtAddr,
					nLength);
	if (nPhysAddr == 0) {
		dprintk(KERN_ERR
			"SCXLNXDeviceMmap(%p) : \
			SCXLNXConnCheckSharedMem failed\n",
			pConn);
		return -EACCES;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	/*erase current cache config and replace it by WRITETHROUGH */
	prot = ((vma->vm_page_prot) & CLEAN_CACHE_CFG_MASK)|
		L_PTE_MT_WRITETHROUGH;
#else
	/*remove bufferable, add cacheable */
	prot = ((vma->vm_page_prot) ^ L_PTE_BUFFERABLE) | L_PTE_CACHEABLE;
#endif
	dprintk(KERN_INFO
		"SCXLNXDeviceMmap(%p) : VA=0x%x -> PA=0x%x, prot=0x%x\n",
		pConn,
		(u32) pVirtAddr, nPhysAddr, (unsigned int)prot);
#ifndef NDEBUG
	{
		unsigned int pa, inner, outter;
		pa = VA2PA(pVirtAddr, &inner, &outter);
		dprintk(KERN_INFO
			"%x/%x/%x/%x [va/pa/inner/outter] old prot=%x\n",
			(unsigned int)pVirtAddr, pa, inner, outter,
			(unsigned int)(vma->vm_page_prot));
	}
#endif

	nError = remap_pfn_range(vma,
				 vma->vm_start,
				 nPhysAddr >> PAGE_SHIFT, nLength, prot);
	if (nError) {
		dprintk(KERN_ERR "SCXLNXDeviceMmap(%p) :\
			Mapping failed [%d]\n",
			pConn, nError);
		return -EAGAIN;
	}

#ifndef NDEBUG
	SCXLNXDumpAttributes_CP15(vma->vm_start);
#endif
	vma->vm_ops = &g_sSCXLNXMmops;

	static_SCXLNXVmaOpen(vma);

	return 0;
}

/*-------------------------------------------------------------------------- */

module_init(SCXLNXDeviceRegister);
/*
 *There is no way to unload the driver as the API_HAL_SDP_RUNTIME_INIT call
 *can be called only once.
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trusted Logic S.A.");

/*-------------------------------------------------------------------------- */

#if defined(CRYPTOKI_KERNEL_OMAP3XXX)

#include "scxlnx_driver.h"

int SCXLNX_driver_DeviceOpen(struct inode *inode, struct file *file)
{
	return SCXLNXDeviceOpen(inode, file);
}

int SCXLNX_driver_DeviceIoctl(struct inode *inode, struct file *file,
				unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	return SCXLNXDeviceIoctl(file, ioctl_num, ioctl_param);
}

int SCXLNX_driver_DeviceRelease(struct inode *inode, struct file *file)
{
	return SCXLNXDeviceRelease(inode, file);
}

#endif
