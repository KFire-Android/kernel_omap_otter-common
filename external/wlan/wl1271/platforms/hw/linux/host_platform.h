/*
 * host_platform.h
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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

/*--------------------------------------------------------------------------
 Module:      host_platform_sdio.h

 Purpose:     This module defines unified interface to the host platform specific
              sources and services.

--------------------------------------------------------------------------*/

#ifndef __HOST_PLATFORM_SDIO__H__
#define __HOST_PLATFORM_SDIO__H__

#include <mach/hardware.h>


#define OMAP_HSMMC1_BASE		0x4809C000
#define OMAP_HSMMC2_BASE		0x480B4000
#define OMAP_HSMMC3_BASE		0x480AD000

#if 0
#define CONTROL_PADCONF_CAM_D1		0x48002118	/* WLAN_EN */
#define CONTROL_PADCONF_MCBSP1_CLKX	0x48002198	/* WLAN_IRQ */

#define CONTROL_PADCONF_MMC3_CLK   	0x480025D8	/* mmc3_cmd */
#define CONTROL_PADCONF_MMC3_CMD   	0x480021D0	/* mmc3_cmd */

#define CONTROL_PADCONF_MMC3_DAT0	0x480025E4	/* mmc3_dat0, mmc3_dat1 */
#define CONTROL_PADCONF_MMC3_DAT2	0x480025E8	/* mmc3_dat2 */
#define CONTROL_PADCONF_MMC3_DAT3	0x480025E0	/* mmc3_dat3 */
#endif

#define INT_MMC2_IRQ			86
#define INT_MMC3_IRQ			94

/* Zoom2 */
#define PMENA_GPIO                      101
#define IRQ_GPIO                        162

/* Sholes */
/*
#define PMENA_GPIO                      186
#define IRQ_GPIO                        65
*/

#define TNETW_IRQ                       (OMAP_GPIO_IRQ(IRQ_GPIO))
#define TIWLAN_IRQ_POLL_INTERVAL	HZ/100
#define HZ_IN_MSEC			HZ/1000
#define TIWLAN_IRQ_POLL_INTERVAL_MS	TIWLAN_IRQ_POLL_INTERVAL/HZ_IN_MSEC

int 
hPlatform_initInterrupt(
	void* tnet_drv,
	void* handle_add
	);

void*
hPlatform_hwGetRegistersAddr(
    TI_HANDLE OsContext
    );

void*
hPlatform_hwGetMemoryAddr(
    TI_HANDLE OsContext
    );

void hPlatform_freeInterrupt(void *tnet_drv);

int  hPlatform_hardResetTnetw(void);
int  hPlatform_Wlan_Hardware_Init(void *tnet_drv);
void hPlatform_Wlan_Hardware_DeInit(void);
int  hPlatform_DevicePowerOff(void);
int  hPlatform_DevicePowerOffSetLongerDelay(void);
int  hPlatform_DevicePowerOn(void);
#endif /* __HOST_PLATFORM_SDIO__H__ */
