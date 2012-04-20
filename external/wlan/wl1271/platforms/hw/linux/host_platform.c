/*
 * host_platform.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
 * All rights reserved.                                                  
 *                                                                       
 * Redistribution and use in source and binary forms, with or without    
 * modification, are permitted provided that the following conditions    
 * are met:                                                              
 *                                                                       
 *  * Redistributions of source code must retain the above copyright     
 *    notice, this list of conditions and the following disclaimer.      
 *  * Redistributions in binary form must reproduce the above copyright  
 *    notice, this list of conditions and the following disclaimer in    
 *    the documentation and/or other materials provided with the         
 *    distribution.                                                      
 *  * Neither the name Texas Instruments nor the names of its            
 *    contributors may be used to endorse or promote products derived    
 *    from this software without specific prior written permission.      
 *                                                                       
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT      
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "tidef.h"
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/wifi_tiwlan.h>

#include "host_platform.h"
#include "ioctl_init.h"
#include "WlanDrvIf.h"
#include "Device1273.h"

#define OS_API_MEM_ADDR		0x0000000
#define OS_API_REG_ADDR		0x0300000
#define SDIO_ATTEMPT_LONGER_DELAY_LINUX  150

static struct wifi_platform_data *wifi_control_data = NULL;
static struct resource *wifi_irqres = NULL;

static int wifi_probe( struct platform_device *pdev )
{
	struct wifi_platform_data *wifi_ctrl = (struct wifi_platform_data *)(pdev->dev.platform_data);

	/* printk("%s\n", __FUNCTION__); */
	wifi_irqres = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "device_wifi_irq");
#if 0
	if (wifi_irqres) {
		printk("wifi_irqres->start = %lu\n", (unsigned long)(wifi_irqres->start));
		printk("wifi_irqres->flags = %lx\n", wifi_irqres->flags);
	}
#endif
	if( wifi_ctrl ) {
		wifi_control_data = wifi_ctrl;
#if 0
		if( wifi_ctrl->set_power )
			wifi_ctrl->set_power(1);	/* Power On */
		if( wifi_ctrl->set_reset )
			wifi_ctrl->set_reset(0);	/* Reset clear */
		if( wifi_ctrl->set_carddetect )
			wifi_ctrl->set_carddetect(1);	/* CardDetect (0->1) */
#endif
	}
	return 0;
}
											
static int wifi_remove( struct platform_device *pdev )
{
	struct wifi_platform_data *wifi_ctrl = (struct wifi_platform_data *)(pdev->dev.platform_data);

	/* printk("%s\n", __FUNCTION__); */
	if( wifi_ctrl ) {
		if( wifi_ctrl->set_carddetect )
			wifi_ctrl->set_carddetect(0);	/* CardDetect (1->0) */
		if( wifi_ctrl->set_reset )
			wifi_ctrl->set_reset(1);	/* Reset active */
		if( wifi_ctrl->set_power )
			wifi_ctrl->set_power(0);	/* Power Off */
	}
	return 0;
}

static struct platform_driver wifi_device = {
	.probe          = wifi_probe,
	.remove         = wifi_remove,
	.suspend        = NULL,
	.resume         = NULL,
	.driver         = {
		.name   = "device_wifi",
	},
};

static int wifi_add_dev( void )
{
	/* printk("%s\n", __FUNCTION__); */
	return platform_driver_register( &wifi_device );
}

static void wifi_del_dev( void )
{
	/* printk("%s\n", __FUNCTION__); */
	platform_driver_unregister( &wifi_device );
}

int wifi_set_carddetect( int on )
{
	/* printk("%s = %d\n", __FUNCTION__, on); */
	if( wifi_control_data && wifi_control_data->set_carddetect ) {
		wifi_control_data->set_carddetect(on);
	}
	return 0;
}

int wifi_set_power( int on, unsigned long msec )
{
	/* printk("%s = %d\n", __FUNCTION__, on); */
	if( wifi_control_data && wifi_control_data->set_power ) {
		wifi_control_data->set_power(on);
	}
	else {
		gpio_set_value(PMENA_GPIO, on);
	}
	if( msec )
		mdelay(msec);
	return 0;
}

int wifi_set_reset( int on, unsigned long msec )
{
	/* printk("%s = %d\n", __FUNCTION__, on); */
	if( wifi_control_data && wifi_control_data->set_reset ) {
		wifi_control_data->set_reset(on);
	}
	if( msec )
		mdelay(msec);
	return 0;
}

/*-----------------------------------------------------------------------------
Routine Name: hPlatform_hardResetTnetw
Routine Description: set the GPIO to low after awaking the TNET from ELP.
Arguments: None
Return Value: 0 - Ok
-----------------------------------------------------------------------------*/

int hPlatform_hardResetTnetw( void )
{
	int err;

	/* Turn power OFF */
	if ((err = wifi_set_power(0, 15)) == 0) {
		/* Turn power ON*/
		err = wifi_set_power(1, 70);
	}
	return err;
} /* hPlatform_hardResetTnetw() */

/* Turn device power off */
int hPlatform_DevicePowerOff( void )
{
	int err;

	err = wifi_set_power(0, 15);
	return err;
}


/* Turn device power off according to a given delay */
int hPlatform_DevicePowerOffSetLongerDelay(void)
{
    int err;
    
    err = wifi_set_power(0, SDIO_ATTEMPT_LONGER_DELAY_LINUX);
    
    return err;
}

/* Turn device power on */
int hPlatform_DevicePowerOn( void )
{
	int err;

	wifi_set_power(1, 15); /* Fixed power sequence */
	wifi_set_power(0, 1);
	/* Should not be changed, 50 msec cause failures */
	err = wifi_set_power(1, 70);
	return err;
}

/*---------------------------------------------------------------------------*/

int hPlatform_Wlan_Hardware_Init(void *tnet_drv)
{
	TWlanDrvIfObj *drv = tnet_drv;

	printk("%s\n", __FUNCTION__);
	wifi_add_dev();
	if (wifi_irqres) {
		drv->irq = wifi_irqres->start;
		drv->irq_flags = wifi_irqres->flags & IRQF_TRIGGER_MASK;
	}
	else {
		drv->irq = TNETW_IRQ;
		drv->irq_flags = (unsigned long)IRQF_TRIGGER_FALLING;
	}
	return 0;
}

/*-----------------------------------------------------------------------------

Routine Name:

        InitInterrupt

Routine Description:

        this function init the interrupt to the Wlan ISR routine.

Arguments:

        tnet_drv - Golbal Tnet driver pointer.

Return Value:

        status

-----------------------------------------------------------------------------*/

int hPlatform_initInterrupt( void *tnet_drv, void* handle_add )
{
	TWlanDrvIfObj *drv = tnet_drv;
	int rc;
	
	if (drv->irq == 0 || handle_add == NULL)
	{
		print_err("hPlatform_initInterrupt() bad param drv->irq=%d handle_add=0x%x !!!\n",drv->irq,(int)handle_add);
		return -EINVAL;
	}
	printk("drv->irq = %u, %lx\n", drv->irq, drv->irq_flags);
	if ((rc = request_irq(drv->irq, handle_add, drv->irq_flags, drv->netdev->name, drv)))
	{
		print_err("TIWLAN: Failed to register interrupt handler\n");
		return rc;
	}
	set_irq_wake(drv->irq, 1);	
	return rc;

} /* hPlatform_initInterrupt() */

/*--------------------------------------------------------------------------------------*/

void hPlatform_freeInterrupt( void *tnet_drv )
{
	TWlanDrvIfObj *drv = tnet_drv;

	set_irq_wake(drv->irq, 0);
	free_irq(drv->irq, drv);
}

/****************************************************************************************
 *                        hPlatform_hwGetRegistersAddr()                                 
 ****************************************************************************************
DESCRIPTION:	

ARGUMENTS:		

RETURN:			

NOTES:         	
*****************************************************************************************/
void *hPlatform_hwGetRegistersAddr(TI_HANDLE OsContext)
{
	return (void *)OS_API_REG_ADDR;
}

/****************************************************************************************
 *                        hPlatform_hwGetMemoryAddr()                                 
 ****************************************************************************************
DESCRIPTION:	

ARGUMENTS:		

RETURN:			

NOTES:         	
*****************************************************************************************/
void *hPlatform_hwGetMemoryAddr(TI_HANDLE OsContext)
{
	return (void *)OS_API_MEM_ADDR;
}


void hPlatform_Wlan_Hardware_DeInit(void)
{
	wifi_del_dev();
}
