/*
 * Device.h
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


/****************************************************************************
 *
 *   MODULE:  Device.h
 *   PURPOSE: Contains Wlan hardware registers defines/structures
 *   
 ****************************************************************************/

#ifndef DEVICE_H
#define DEVICE_H

#include "Device1273.h"


#define ACX_PHI_CCA_THRSH_ENABLE_ENERGY_D	0x140A
#define ACX_PHI_CCA_THRSH_DISABLE_ENERGY_D	0xFFEF

/*
 * Wlan hardware Registers.
 */

/*======================================================================
                      Interrupt Registers                               
=======================================================================*/

#define ACX_REG_INTERRUPT_TRIG		( INT_TRIG )

#define ACX_REG_INTERRUPT_TRIG_H	 ( INT_TRIG_H )

/*=============================================
  Host Interrupt Mask Register - 32bit (RW)
  ------------------------------------------  
  Setting a bit in this register masks the
  corresponding interrupt to the host.
  0 - RX0		- Rx first dubble buffer Data Interrupt
  1 - TXD		- Tx Data Interrupt
  2 - TXXFR		- Tx Transfer Interrupt
  3 - RX1		- Rx second dubble buffer Data Interrupt
  4 - RXXFR		- Rx Transfer Interrupt
  5 - EVENT_A	- Event Mailbox interrupt
  6 - EVENT_B	- Event Mailbox interrupt
  7 - WNONHST	- Wake On Host Interrupt
  8 - TRACE_A	- Debug Trace interrupt
  9 - TRACE_B	- Debug Trace interrupt
 10 - CDCMP		- Command Complete Interrupt
 11 -
 12 -
 13 -
 14 - ICOMP		- Initialization Complete Interrupt
 16 - SG SE		- Soft Gemini - Sense enable interrupt
 17 - SG SD		- Soft Gemini - Sense disable interrupt
 18 -			- 
 19 -			- 
 20 -			- 
 21-			- 
 Default: 0x0001
*==============================================*/
#define ACX_REG_INTERRUPT_MASK				( HINT_MASK )

/*=============================================
  Host Interrupt Mask Set 16bit, (Write only) 
  ------------------------------------------  
 Setting a bit in this register sets          
 the corresponding bin in ACX_HINT_MASK register
 without effecting the mask                   
 state of other bits (0 = no effect).         
==============================================*/
#define ACX_HINT_MASK_SET_REG          HINT_MASK_SET

/*=============================================
  Host Interrupt Mask Clear 16bit,(Write only)
  ------------------------------------------  
 Setting a bit in this register clears        
 the corresponding bin in ACX_HINT_MASK register
 without effecting the mask                   
 state of other bits (0 = no effect).         
=============================================*/
#define ACX_HINT_MASK_CLR_REG          HINT_MASK_CLR

/*=============================================
  Host Interrupt Status Nondestructive Read   
  16bit,(Read only)                           
  ------------------------------------------  
 The host can read this register to determine 
 which interrupts are active.                 
 Reading this register doesn't                
 effect its content.                          
=============================================*/
#define ACX_REG_INTERRUPT_NO_CLEAR			( HINT_STS_ND )

/*=============================================
  Host Interrupt Status Clear on Read  Register
  16bit,(Read only)                           
  ------------------------------------------  
 The host can read this register to determine 
 which interrupts are active.                 
 Reading this register clears it,             
 thus making all interrupts inactive.         
==============================================*/
#define ACX_REG_INTERRUPT_CLEAR				( HINT_STS_CLR )

/*=============================================
  Host Interrupt Acknowledge Register         
  16bit,(Write only)                          
  ------------------------------------------  
 The host can set individual bits in this     
 register to clear (acknowledge) the corresp. 
 interrupt status bits in the HINT_STS_CLR and
 HINT_STS_ND registers, thus making the       
 assotiated interrupt inactive. (0-no effect) 
==============================================*/
#define ACX_REG_INTERRUPT_ACK				( HINT_ACK )


/*===============================================
   Host Software Reset - 32bit RW 
 ------------------------------------------
    [31:1] Reserved 
        0  SOFT_RESET Soft Reset  - When this bit is set,
         it holds the Wlan hardware in a soft reset state. 
         This reset disables all MAC and baseband processor 
         clocks except the CardBus/PCI interface clock. 
         It also initializes all MAC state machines except 
         the host interface. It does not reload the
         contents of the EEPROM. When this bit is cleared 
         (not self-clearing), the Wlan hardware
         exits the software reset state.
===============================================*/
#define ACX_REG_SLV_SOFT_RESET				( SLV_SOFT_RESET )
	#define SLV_SOFT_RESET_BIT		0x00000001

/*===============================================
 EEPROM Burst Read Start  - 32bit RW 
 ------------------------------------------
 [31:1] Reserved 
     0  ACX_EE_START -  EEPROM Burst Read Start 0
        Setting this bit starts a burst read from 
        the external EEPROM. 
        If this bit is set (after reset) before an EEPROM read/write, 
        the burst read starts at EEPROM address 0.
        Otherwise, it starts at the address 
        following the address of the previous access. 
        TheWlan hardware hardware clears this bit automatically.
        
        Default: 0x00000000
*================================================*/
#define ACX_REG_EE_START					( EE_START )
	#define START_EEPROM_MGR	0x00000001

/*=======================================================================
                        Embedded ARM CPU Control
========================================================================*/
/*===============================================
 Halt eCPU   - 32bit RW 
 ------------------------------------------
    0 HALT_ECPU Halt Embedded CPU - This bit is the 
      compliment of bit 1 (MDATA2) in the SOR_CFG register. 
      During a hardware reset, this bit holds 
      the inverse of MDATA2.
      When downloading firmware from the host, 
      set this bit (pull down MDATA2). 
      The host clears this bit after downloading the firmware into 
      zero-wait-state SSRAM.
      When loading firmware from Flash, clear this bit (pull up MDATA2) 
      so that the eCPU can run the bootloader code in Flash
    HALT_ECPU eCPU State
    --------------------
    1 halt eCPU
    0 enable eCPU
===============================================*/
#define ACX_REG_ECPU_CONTROL				( ECPU_CTRL )


/*=======================================================================
                    Command/Information Mailbox Pointers
========================================================================*/

/*===============================================
   Command Mailbox Pointer - 32bit RW 
 ------------------------------------------
    This register holds the start address of 
    the command mailbox located in the Wlan hardware memory. 
    The host must read this pointer after a reset to 
    find the location of the command mailbox. 
    The Wlan hardware initializes the command mailbox 
    pointer with the default address of the command mailbox.
    The command mailbox pointer is not valid until after 
    the host receives the Init Complete interrupt from 
    the Wlan hardware.
===============================================*/
#define REG_COMMAND_MAILBOX_PTR				( SCR_PAD0 ) 

/*===============================================
   Information Mailbox Pointer - 32bit RW 
 ------------------------------------------
    This register holds the start address of 
    the information mailbox located in the Wlan hardware memory. 
    The host must read this pointer after a reset to find 
    the location of the information mailbox. 
    The Wlan hardware initializes the information mailbox pointer 
    with the default address of the information mailbox. 
    The information mailbox pointer is not valid 
    until after the host receives the Init Complete interrupt from
    the Wlan hardware.
===============================================*/
#define REG_EVENT_MAILBOX_PTR				( SCR_PAD1 ) 


/*=======================================================================
                   Misc
========================================================================*/


#define REG_ENABLE_TX_RX				( IO_CONTROL_ENABLE ) 
/*
 * Rx configuration (filter) information element
 * ---------------------------------------------
 */
#define REG_RX_CONFIG				( RX_CFG ) 
#define REG_RX_FILTER				( RX_FILTER_CFG ) 

#define RX_CFG_ENABLE_PHY_HEADER_PLCP	0x0002	
#define RX_CFG_PROMISCUOUS				0x0008	/* promiscuous - receives all valid frames */
#define RX_CFG_BSSID					0x0020	/* receives frames from any BSSID */
#define RX_CFG_MAC						0x0010	/* receives frames destined to any MAC address */
#define RX_CFG_ENABLE_ONLY_MY_DEST_MAC	0x0010	
#define RX_CFG_ENABLE_ANY_DEST_MAC		0x0000	
#define RX_CFG_ENABLE_ONLY_MY_BSSID		0x0020  
#define RX_CFG_ENABLE_ANY_BSSID			0x0000  
#define RX_CFG_DISABLE_BCAST			0x0200	/* discards all broadcast frames */
#define RX_CFG_ENABLE_ONLY_MY_SSID		0x0400
#define RX_CFG_ENABLE_RX_CMPLT_FCS_ERROR 0x0800
#define RX_CFG_COPY_RX_STATUS			0x2000
#define RX_CFG_TSF						 0x10000

#define RX_CONFIG_OPTION_ANY_DST_MY_BSS		( RX_CFG_ENABLE_ANY_DEST_MAC     | RX_CFG_ENABLE_ONLY_MY_BSSID)
#define RX_CONFIG_OPTION_MY_DST_ANY_BSS		( RX_CFG_ENABLE_ONLY_MY_DEST_MAC | RX_CFG_ENABLE_ANY_BSSID)
#define RX_CONFIG_OPTION_ANY_DST_ANY_BSS	( RX_CFG_ENABLE_ANY_DEST_MAC     | RX_CFG_ENABLE_ANY_BSSID)
#define RX_CONFIG_OPTION_MY_DST_MY_BSS		( RX_CFG_ENABLE_ONLY_MY_DEST_MAC | RX_CFG_ENABLE_ONLY_MY_BSSID)

#define RX_CONFIG_OPTION_FOR_SCAN           ( RX_CFG_ENABLE_PHY_HEADER_PLCP  | RX_CFG_ENABLE_RX_CMPLT_FCS_ERROR | RX_CFG_COPY_RX_STATUS | RX_CFG_TSF)
#define RX_CONFIG_OPTION_FOR_MEASUREMENT    ( RX_CFG_ENABLE_ANY_DEST_MAC )
#define RX_CONFIG_OPTION_FOR_JOIN    		( RX_CFG_ENABLE_ONLY_MY_BSSID | RX_CFG_ENABLE_ONLY_MY_DEST_MAC )
#define RX_CONFIG_OPTION_FOR_IBSS_JOIN    	( RX_CFG_ENABLE_ONLY_MY_SSID  | RX_CFG_ENABLE_ONLY_MY_DEST_MAC )

#define RX_FILTER_OPTION_DEF			( CFG_RX_MGMT_EN | CFG_RX_DATA_EN | CFG_RX_CTL_EN | CFG_RX_RCTS_ACK | CFG_RX_BCN_EN  | CFG_RX_AUTH_EN  | CFG_RX_ASSOC_EN)
#define RX_FILTER_OPTION_FILTER_ALL		0
#define RX_FILTER_OPTION_DEF_PRSP_BCN	( CFG_RX_PRSP_EN | CFG_RX_MGMT_EN | CFG_RX_CTL_EN | CFG_RX_RCTS_ACK | CFG_RX_BCN_EN)
#define RX_FILTER_OPTION_JOIN			( CFG_RX_MGMT_EN | CFG_RX_DATA_EN | CFG_RX_CTL_EN | CFG_RX_BCN_EN   | CFG_RX_AUTH_EN | CFG_RX_ASSOC_EN | CFG_RX_RCTS_ACK | CFG_RX_PRSP_EN)


/*===============================================
   Phy regs
 ===============================================*/
#define ACX_PHY_ADDR_REG                SBB_ADDR
#define ACX_PHY_DATA_REG                SBB_DATA
#define ACX_PHY_CTRL_REG                SBB_CTL
#define ACX_PHY_REG_WR_MASK             0x00000001ul
#define ACX_PHY_REG_RD_MASK             0x00000002ul    


/*===============================================
 EEPROM Read/Write Request 32bit RW 
 ------------------------------------------
 1 EE_READ - EEPROM Read Request 1 - Setting this bit 
   loads a single byte of data into the EE_DATA 
   register from the EEPROM location specified in 
   the EE_ADDR register. 
   The Wlan hardware hardware clears this bit automatically. 
   EE_DATA is valid when this bit is cleared.
 0 EE_WRITE  - EEPROM Write Request  - Setting this bit 
   writes a single byte of data from the EE_DATA register into the
   EEPROM location specified in the EE_ADDR register. 
   The Wlan hardware hardware clears this bit automatically.
*===============================================*/
#define ACX_EE_CTL_REG                      EE_CTL
#define EE_WRITE                            0x00000001ul
#define EE_READ                             0x00000002ul

/*===============================================
 EEPROM Address  - 32bit RW 
 ------------------------------------------
 This register specifies the address 
 within the EEPROM from/to which to read/write data.
===============================================*/
#define ACX_EE_ADDR_REG                     EE_ADDR

/*===============================================
 EEPROM Data  - 32bit RW 
 ------------------------------------------
    This register either holds the read 8 bits of 
    data from the EEPROM or the write data 
    to be written to the EEPROM.
===============================================*/
#define ACX_EE_DATA_REG                     EE_DATA

/*===============================================
 EEPROM Base Address  - 32bit RW 
 ------------------------------------------
    This register holds the upper nine bits 
    [23:15] of the 24-bit Wlan hardware memory 
    address for burst reads from EEPROM accesses. 
    The EEPROM provides the lower 15 bits of this address. 
    The MSB of the address from the EEPROM is ignored.
===============================================*/
#define ACX_EE_CFG                          EE_CFG  

/*===============================================
  GPIO Output Values  -32bit, RW
 ------------------------------------------
    [31:16]  Reserved 
    [15: 0]  Specify the output values (at the output driver inputs) for
             GPIO[15:0], respectively.
===============================================*/
#define ACX_GPIO_OUT_REG            GPIO_OUT
#define ACX_MAX_GPIO_LINES          15

/*===============================================
  Contention window  -32bit, RW
 ------------------------------------------
    [31:26]  Reserved 
    [25:16]  Max (0x3ff)
    [15:07]  Reserved
    [06:00]  Current contention window value - default is 0x1F
===============================================*/
#define ACX_CONT_WIND_CFG_REG    CONT_WIND_CFG
#define ACX_CONT_WIND_MIN_MASK   0x0000007f
#define ACX_CONT_WIND_MAX        0x03ff0000

/*
 * Indirect slave register/memory registers
 * ----------------------------------------
 */
#define HW_SLAVE_REG_ADDR_REG		0x00000004
#define HW_SLAVE_REG_DATA_REG		0x00000008
#define HW_SLAVE_REG_CTRL_REG		0x0000000c

#define SLAVE_AUTO_INC				0x00010000
#define SLAVE_NO_AUTO_INC			0x00000000
#define SLAVE_HOST_LITTLE_ENDIAN	0x00000000

#define HW_SLAVE_MEM_ADDR_REG		SLV_MEM_ADDR
#define HW_SLAVE_MEM_DATA_REG		SLV_MEM_DATA
#define HW_SLAVE_MEM_CTRL_REG		SLV_MEM_CTL
#define HW_SLAVE_MEM_ENDIAN_REG		SLV_END_CTL

#define HW_FUNC_EVENT_INT_EN		0x8000
#define HW_FUNC_EVENT_MASK_REG		0x00000034

#define ACX_MAC_TIMESTAMP_REG	(MAC_TIMESTAMP)

/*===============================================
  HI_CFG Interface Configuration Register Values
 ------------------------------------------
===============================================*/
#define HI_CFG_UART_ENABLE          0x00000004
#define HI_CFG_RST232_ENABLE        0x00000008
#define HI_CFG_CLOCK_REQ_SELECT     0x00000010
#define HI_CFG_HOST_INT_ENABLE      0x00000020
#define HI_CFG_VLYNQ_OUTPUT_ENABLE  0x00000040
#define HI_CFG_HOST_INT_ACTIVE_LOW  0x00000080
#define HI_CFG_UART_TX_OUT_GPIO_15  0x00000100
#define HI_CFG_UART_TX_OUT_GPIO_14  0x00000200
#define HI_CFG_UART_TX_OUT_GPIO_7   0x00000400

/*
 * NOTE: USE_ACTIVE_HIGH compilation flag should be defined in makefile
 *       for platforms using active high interrupt level
 */
#ifdef USE_IRQ_ACTIVE_HIGH
#define HI_CFG_DEF_VAL              \
        HI_CFG_UART_ENABLE |        \
        HI_CFG_RST232_ENABLE |      \
        HI_CFG_CLOCK_REQ_SELECT |   \
        HI_CFG_HOST_INT_ENABLE
#else
#define HI_CFG_DEF_VAL              \
        HI_CFG_UART_ENABLE |        \
        HI_CFG_RST232_ENABLE |      \
        HI_CFG_CLOCK_REQ_SELECT |   \
        HI_CFG_HOST_INT_ENABLE |    \
        HI_CFG_HOST_INT_ACTIVE_LOW
#endif

#endif   /* DEVICE_H */

