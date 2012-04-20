/*
 * Device1273.h
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


/**********************************************************************************************************************

  FILENAME:       Device1273.h

  DESCRIPTION:    TNETW1273 Registes addresses/defintion



***********************************************************************************************************************/

#ifndef DEVICE1273_H
#define DEVICE1273_H


/* Base addresses*/
/* They are not used inside registers definition in purpose to allow this header file*/
/* to be used as an easy reference to register -> address date base. Keep this as it*/
/* is very powerful for debugging purpose.*/
#define REGISTERS_BASE	0x00300000
#define INT_BASE		0x00300400
#define REG_CONFIG_BASE	0x00300800
#define CLK_BASE		0x00300C00
#define SDMA_BASE		0x00301000
#define AES_BASE		0x00301400
#define WEP_BASE		0x00301800
#define TKIP_BASE		0x00301C00
#define SEEPROM_BASE	0x00302000
#define PAR_HOST_BASE	0x00302400
#define SDIO_BASE		0x00302800
#define UART_BASE		0x00302C00
#define USB11_BASE		0x00304000
#define LDMA_BASE		0x00304400
#define RX_BASE			0x00304800
#define ACCESS_BASE		0x00304c00
#define TX_BASE			0x00305000
#define RMAC_CSR_BASE	0x00305400
#define AFE_PM			0x00305800
#define VLYNQ_BASE		0x00308000
#define PCI_BASE		0x00308400
#define USB20_BASE		0x0030A000
#define DRPW_BASE		0x00310000
#define PHY_BASE		0x003C0000

/* DRPw init scratch register */
#define DRPW_SCRATCH_START             (DRPW_BASE + 0x002C)

/* System DMA registers*/
/* Order of registers was changed*/
#define DMA_GLB_CFG                    (REGISTERS_BASE + 0x1000)
#define DMA_HDESC_OFFSET               (REGISTERS_BASE + 0x1004)
#define DMA_HDATA_OFFSET               (REGISTERS_BASE + 0x1008)
#define DMA_CFG0                       (REGISTERS_BASE + 0x100C) /* SDMA_HOST_CFG0 changed*/
#define DMA_CTL0                       (REGISTERS_BASE + 0x1010) /* SDMA_CTRL0 changed*/
#define DMA_LENGTH0                    (REGISTERS_BASE + 0x1014)
#define DMA_L_ADDR0                    (REGISTERS_BASE + 0x1018) /* SDMA_RD_ADDR ?*/
#define DMA_L_PTR0                     (REGISTERS_BASE + 0x101C) /* SDMA_RD_OFFSET ?*/
#define DMA_H_ADDR0                    (REGISTERS_BASE + 0x1020) /* SDMA_WR_ADDR ?*/
#define DMA_H_PTR0                     (REGISTERS_BASE + 0x1024) /* SDMA_WR_OFFSET ?*/
#define DMA_STS0                       (REGISTERS_BASE + 0x1028) /* Changed*/
#define DMA_CFG1                       (REGISTERS_BASE + 0x1030) /* SDMA_HOST_CFG1 changed*/
#define DMA_CTL1                       (REGISTERS_BASE + 0x1034) /* SDMA_CTRL1 changed*/
#define DMA_LENGTH1                    (REGISTERS_BASE + 0x1038)
#define DMA_L_ADDR1                    (REGISTERS_BASE + 0x103C)
#define DMA_L_PTR1                     (REGISTERS_BASE + 0x1040)
#define DMA_H_ADDR1                    (REGISTERS_BASE + 0x1044)
#define DMA_H_PTR1                     (REGISTERS_BASE + 0x1048)
#define DMA_STS1                       (REGISTERS_BASE + 0x104C)
#define DMA_HFRM_PTR                   (REGISTERS_BASE + 0x1050) /* New ?*/
#define DMA_DEBUG                      (REGISTERS_BASE + 0x1054) /* Changed*/

/* Local DMA registers*/
/* number changed from 4 to 2*/
#define LDMA_DEBUG                     (REGISTERS_BASE + 0x4400)
#define LDMA_CTL0                      (REGISTERS_BASE + 0x4404) /* Add 2 bits to support fix address (FIFO)*/
#define LDMA_STATUS0                   (REGISTERS_BASE + 0x4408)
#define LDMA_LENGTH0                   (REGISTERS_BASE + 0x440c)
#define LDMA_RD_ADDR0                  (REGISTERS_BASE + 0x4410)
#define LDMA_RD_OFFSET0                (REGISTERS_BASE + 0x4414)
#define LDMA_WR_ADDR0                  (REGISTERS_BASE + 0x4418)
#define LDMA_WR_OFFSET0                (REGISTERS_BASE + 0x441c)
#define LDMA_CTL1                      (REGISTERS_BASE + 0x4428) /* Add 2 bits to support fix address (FIFO)*/
#define LDMA_STATUS1                   (REGISTERS_BASE + 0x442c)
#define LDMA_LENGTH1                   (REGISTERS_BASE + 0x4430)
#define LDMA_RD_ADDR1                  (REGISTERS_BASE + 0x4434)
#define LDMA_RD_OFFSET1                (REGISTERS_BASE + 0x4438)
#define LDMA_WR_ADDR1                  (REGISTERS_BASE + 0x443c)
#define LDMA_WR_OFFSET1                (REGISTERS_BASE + 0x4440)
/* For TNETW compatability (if willbe )*/
#define LDMA_CUR_RD_PTR0               LDMA_RD_ADDR0
#define LDMA_CUR_WR_PTR0               LDMA_WR_ADDR0
#define LDMA_CUR_RD_PTR1               LDMA_RD_ADDR1
#define LDMA_CUR_WR_PTR1               LDMA_WR_ADDR1

/* Host Slave registers*/
#define SLV_SOFT_RESET                 (REGISTERS_BASE + 0x0000) /* self clearing*/
#define SLV_REG_ADDR                   (REGISTERS_BASE + 0x0004)
#define SLV_REG_DATA                   (REGISTERS_BASE + 0x0008)
#define SLV_REG_ADATA                  (REGISTERS_BASE + 0x000c)
#define SLV_MEM_CP                     (REGISTERS_BASE + 0x0010)
#define SLV_MEM_ADDR                   (REGISTERS_BASE + 0x0014)
#define SLV_MEM_DATA                   (REGISTERS_BASE + 0x0018)
#define SLV_MEM_CTL                    (REGISTERS_BASE + 0x001c) /* bit 19 moved to PCMCIA_CTL*/
#define SLV_END_CTL                    (REGISTERS_BASE + 0x0020) /* 2 bits moved to ENDIAN_CTL*/

/* Timer registers*/
/* Timer1/2 count MAC clocks*/
/* Timer3/4/5 count usec*/
#define TIM1_CTRL                      (REGISTERS_BASE + 0x0918)
#define TIM1_LOAD                      (REGISTERS_BASE + 0x091C)
#define TIM1_CNT                       (REGISTERS_BASE + 0x0920)
#define TIM2_CTRL                      (REGISTERS_BASE + 0x0924)
#define TIM2_LOAD                      (REGISTERS_BASE + 0x0928)
#define TIM2_CNT                       (REGISTERS_BASE + 0x092C)
#define TIM3_CTRL                      (REGISTERS_BASE + 0x0930)
#define TIM3_LOAD                      (REGISTERS_BASE + 0x0934)
#define TIM3_CNT                       (REGISTERS_BASE + 0x0938)
#define TIM4_CTRL                      (REGISTERS_BASE + 0x093C)
#define TIM4_LOAD                      (REGISTERS_BASE + 0x0940)
#define TIM4_CNT                       (REGISTERS_BASE + 0x0944)
#define TIM5_CTRL                      (REGISTERS_BASE + 0x0948)
#define TIM5_LOAD                      (REGISTERS_BASE + 0x094C)
#define TIM5_CNT                       (REGISTERS_BASE + 0x0950)

/* Watchdog registers*/
#define WDOG_CTRL                      (REGISTERS_BASE + 0x0954)
#define WDOG_LOAD                      (REGISTERS_BASE + 0x0958)
#define WDOG_CNT                       (REGISTERS_BASE + 0x095C)
#define WDOG_STS                       (REGISTERS_BASE + 0x0960)
#define WDOG_FEED                      (REGISTERS_BASE + 0x0964)

/* Interrupt registers*/
/* 64 bit interrupt sources registers ws ced. sme interupts were removed and new ones were added*/
/* Order was changed*/
#define FIQ_MASK                       (REGISTERS_BASE + 0x0400)
#define FIQ_MASK_L                     (REGISTERS_BASE + 0x0400)
#define FIQ_MASK_H                     (REGISTERS_BASE + 0x0404)
#define FIQ_MASK_SET                   (REGISTERS_BASE + 0x0408)
#define FIQ_MASK_SET_L                 (REGISTERS_BASE + 0x0408)
#define FIQ_MASK_SET_H                 (REGISTERS_BASE + 0x040C)
#define FIQ_MASK_CLR                   (REGISTERS_BASE + 0x0410)
#define FIQ_MASK_CLR_L                 (REGISTERS_BASE + 0x0410)
#define FIQ_MASK_CLR_H                 (REGISTERS_BASE + 0x0414)
#define IRQ_MASK                       (REGISTERS_BASE + 0x0418)
#define IRQ_MASK_L                     (REGISTERS_BASE + 0x0418)
#define IRQ_MASK_H                     (REGISTERS_BASE + 0x041C)
#define IRQ_MASK_SET                   (REGISTERS_BASE + 0x0420)
#define IRQ_MASK_SET_L                 (REGISTERS_BASE + 0x0420)
#define IRQ_MASK_SET_H                 (REGISTERS_BASE + 0x0424)
#define IRQ_MASK_CLR                   (REGISTERS_BASE + 0x0428)
#define IRQ_MASK_CLR_L                 (REGISTERS_BASE + 0x0428)
#define IRQ_MASK_CLR_H                 (REGISTERS_BASE + 0x042C)
#define ECPU_MASK                      (REGISTERS_BASE + 0x0448)
#define FIQ_STS_L                      (REGISTERS_BASE + 0x044C)
#define FIQ_STS_H                      (REGISTERS_BASE + 0x0450)
#define IRQ_STS_L                      (REGISTERS_BASE + 0x0454)
#define IRQ_STS_H                      (REGISTERS_BASE + 0x0458)
#define INT_STS_ND                     (REGISTERS_BASE + 0x0464)
#define INT_STS_RAW_L                  (REGISTERS_BASE + 0x0464)
#define INT_STS_RAW_H                  (REGISTERS_BASE + 0x0468)
#define INT_STS_CLR                    (REGISTERS_BASE + 0x04B4)
#define INT_STS_CLR_L                  (REGISTERS_BASE + 0x04B4)
#define INT_STS_CLR_H                  (REGISTERS_BASE + 0x04B8)
#define INT_ACK                        (REGISTERS_BASE + 0x046C)
#define INT_ACK_L                      (REGISTERS_BASE + 0x046C)
#define INT_ACK_H                      (REGISTERS_BASE + 0x0470)
#define INT_TRIG                       (REGISTERS_BASE + 0x0474)
#define INT_TRIG_L                     (REGISTERS_BASE + 0x0474)
#define INT_TRIG_H                     (REGISTERS_BASE + 0x0478)
#define HOST_STS_L                     (REGISTERS_BASE + 0x045C)
#define HOST_STS_H                     (REGISTERS_BASE + 0x0460)
#define HOST_MASK                      (REGISTERS_BASE + 0x0430)
#define HOST_MASK_L                    (REGISTERS_BASE + 0x0430)
#define HOST_MASK_H                    (REGISTERS_BASE + 0x0434)
#define HOST_MASK_SET                  (REGISTERS_BASE + 0x0438)
#define HOST_MASK_SET_L                (REGISTERS_BASE + 0x0438)
#define HOST_MASK_SET_H                (REGISTERS_BASE + 0x043C)
#define HOST_MASK_CLR                  (REGISTERS_BASE + 0x0440)
#define HOST_MASK_CLR_L                (REGISTERS_BASE + 0x0440)
#define HOST_MASK_CLR_H                (REGISTERS_BASE + 0x0444)

/* GPIO Interrupts*/
#define GPIO_INT_STS                   (REGISTERS_BASE + 0x0484) /* 22 GPIOs*/
#define GPIO_INT_ACK                   (REGISTERS_BASE + 0x047C)
#define GPIO_INT_MASK                  (REGISTERS_BASE + 0x0480)
#define GPIO_POS_MASK                  (REGISTERS_BASE + 0x04BC) /* New*/
#define GPIO_NEG_MASK                  (REGISTERS_BASE + 0x04C0) /* New*/

/* Protocol Interrupts*/
#define PROTO_INT_STS                  (REGISTERS_BASE + 0x0490) /* Add 2 PHY->MAC source interrupts*/
#define PROTO_INT_ACK                  (REGISTERS_BASE + 0x0488)
#define PROTO_INT_MASK                 (REGISTERS_BASE + 0x048C)

/* Host Interrupts - The following Addresses are for 1273 */
#define HINT_MASK                      (REGISTERS_BASE + 0x04DC)
#define HINT_MASK_SET                  (REGISTERS_BASE + 0x04E0)
#define HINT_MASK_CLR                  (REGISTERS_BASE + 0x04E4)
#define HINT_STS_ND_MASKED             (REGISTERS_BASE + 0x04EC)
#define HINT_STS_ND  		           (REGISTERS_BASE + 0x04E8)	/* 1150 spec calls this HINT_STS_RAW*/
#define HINT_STS_CLR                   (REGISTERS_BASE + 0x04F8)
#define HINT_ACK                       (REGISTERS_BASE + 0x04F0)
#define HINT_TRIG                      (REGISTERS_BASE + 0x04F4)

/* Clock registers*/
#define CLK_CFG                        (REGISTERS_BASE + 0x0C00) /* new ARM clock bit */
#define CLK_CTRL                       (REGISTERS_BASE + 0x0C04) /* changed*/
#define BLK_RST                        (REGISTERS_BASE + 0x0C08) /* changed*/
#define CFG_USEC_STB                   (REGISTERS_BASE + 0x0C0C)
#define ARM_GATE_CLK_REG               (REGISTERS_BASE + 0x0C10) /* new*/
#define BUSY_STAT_REG                  (REGISTERS_BASE + 0x0C14) /* new*/
#define CFG_PHY_CLK88                  (REGISTERS_BASE + 0x0C18)
#define DYNAMIC_CLKGATE                (REGISTERS_BASE + 0x0C1C) /* new*/

/* AES registers*/
/* Major changes to this module*/
#define AES_START                      (REGISTERS_BASE + 0x1400)
#define AES_CFG                        (REGISTERS_BASE + 0x1404)
#define AES_CTL                        (REGISTERS_BASE + 0x1408)
#define AES_STATUS                     (REGISTERS_BASE + 0x140C)
#define AES_LENGTH                     (REGISTERS_BASE + 0x1410)
#define AES_RD_ADDR                    (REGISTERS_BASE + 0x1414)
#define AES_RD_OFFSET                  (REGISTERS_BASE + 0x1418)
#define AES_WR_ADDR                    (REGISTERS_BASE + 0x141C)
#define AES_WR_OFFSET                  (REGISTERS_BASE + 0x1420)
#define AES_CUR_RD_PTR                 (REGISTERS_BASE + 0x1424)
#define AES_CUR_WR_PTR                 (REGISTERS_BASE + 0x1428)
#define AES_KEY_0                      (REGISTERS_BASE + 0x142C)
#define AES_KEY_1                      (REGISTERS_BASE + 0x1430)
#define AES_KEY_2                      (REGISTERS_BASE + 0x1434)
#define AES_KEY_3                      (REGISTERS_BASE + 0x1438)
#define AES_NONCE_0                    (REGISTERS_BASE + 0x143C)
#define AES_NONCE_1                    (REGISTERS_BASE + 0x1440)
#define AES_NONCE_2                    (REGISTERS_BASE + 0x1444)
#define AES_NONCE_3                    (REGISTERS_BASE + 0x1448)
#define AES_MIC_0                      (REGISTERS_BASE + 0x144C)
#define AES_MIC_1                      (REGISTERS_BASE + 0x1450)
#define AES_MIC_2                      (REGISTERS_BASE + 0x1454)
#define AES_MIC_3                      (REGISTERS_BASE + 0x1458)
#define AES_ASSO_DATA_0                (REGISTERS_BASE + 0x145C)
#define AES_ASSO_DATA_1                (REGISTERS_BASE + 0x1460)
#define AES_ASSO_DATA_2                (REGISTERS_BASE + 0x1464)
#define AES_ASSO_DATA_3                (REGISTERS_BASE + 0x1468)
#define AES_NUM_OF_ROUNDS              (REGISTERS_BASE + 0x146C)
#define AES_TX_QUEUE_PTR               (REGISTERS_BASE + 0x1470)
#define AES_RX_QUEUE_PTR               (REGISTERS_BASE + 0x1474)
#define AES_STACK                      (REGISTERS_BASE + 0x1478)
#define AES_INT_RAW                    (REGISTERS_BASE + 0x147C)
#define AES_INT_MASK                   (REGISTERS_BASE + 0x1480)
#define AES_INT_STS                    (REGISTERS_BASE + 0x1484)

/* WEP registers*/
/* Order was changed*/
#define DEC_CTL                        (REGISTERS_BASE + 0x1800)
#define DEC_STATUS                     (REGISTERS_BASE + 0x1804)
#define DEC_MBLK                       (REGISTERS_BASE + 0x1808)
#define DEC_KEY_ADDR                   (REGISTERS_BASE + 0x180C)
#define DEC_KEY_LEN                    (REGISTERS_BASE + 0x1810)
#define DEC_ADDR_UPPER_BYTE            (REGISTERS_BASE + 0x1814) /* new*/
#define DEC_LEN                        (REGISTERS_BASE + 0x1818)
#define DEC_OFFSET                     (REGISTERS_BASE + 0x181C)
#define DEC_WR_MBLK                    (REGISTERS_BASE + 0x1820)
#define DEC_WR_OFFSET                  (REGISTERS_BASE + 0x1824)

/* TKIP MICHAEL reisters*/
/* order changed*/
#define MCHL_START0                    (REGISTERS_BASE + 0x1C00)
#define MCHL_DMV_START_MBLK0           (REGISTERS_BASE + 0x1C04) /* Changed to 23:5 format*/
#define MCHL_DMV_CUR_MBLK0             (REGISTERS_BASE + 0x1C10)
#define MCHL_DMV_OFFSET0               (REGISTERS_BASE + 0x1C08)
#define MCHL_DMV_LENGTH0               (REGISTERS_BASE + 0x1C0C)
#define MCHL_DMV_CFG0                  (REGISTERS_BASE + 0x1C14)
#define MCHL_KEY_L0                    (REGISTERS_BASE + 0x1C18)
#define MCHL_KEY_H0                    (REGISTERS_BASE + 0x1C1C)
#define MCHL_MIC_L0                    (REGISTERS_BASE + 0x1C20)
#define MCHL_MIC_H0                    (REGISTERS_BASE + 0x1C24)
#define MCHL_START1                    (REGISTERS_BASE + 0x1C28)
#define MCHL_DMV_START_MBLK1           (REGISTERS_BASE + 0x1C2C) /* Changed to 23:5 format*/
#define MCHL_DMV_CUR_MBLK1             (REGISTERS_BASE + 0x1C38)
#define MCHL_DMV_OFFSET1               (REGISTERS_BASE + 0x1C30)
#define MCHL_DMV_LENGTH1               (REGISTERS_BASE + 0x1C34)
#define MCHL_DMV_CFG1                  (REGISTERS_BASE + 0x1C3C)
#define MCHL_KEY_L1                    (REGISTERS_BASE + 0x1C40)
#define MCHL_KEY_H1                    (REGISTERS_BASE + 0x1C44)
#define MCHL_MIC_L1                    (REGISTERS_BASE + 0x1C48)
#define MCHL_MIC_H1                    (REGISTERS_BASE + 0x1C4C)
#define MCHL_CTL0                      (REGISTERS_BASE + 0x1C50) /* new name MCHL_CTRL0*/
#define MCHL_CTL1                      (REGISTERS_BASE + 0x1C54) /* new name MCHL_CTRL1*/
#define MCHL_UPPER_BYTE_ADDR0          (REGISTERS_BASE + 0x1C58) /* new*/
#define MCHL_UPPER_BYTE_ADDR1          (REGISTERS_BASE + 0x1C5C) /* new*/

/* SEEPROM registers*/
#define EE_CFG                         (REGISTERS_BASE + 0x0820)
#define EE_CTL                         (REGISTERS_BASE + 0x2000)
#define EE_DATA                        (REGISTERS_BASE + 0x2004)
#define EE_ADDR                        (REGISTERS_BASE + 0x2008)

/* Parallel Host (PCI/CARDBUS/PCMCIA/GS*/
#define CIS_LADDR                      (REGISTERS_BASE + 0x2400)
#define HI_CTL                         (REGISTERS_BASE + 0x2404)
#define LPWR_MGT                       (REGISTERS_BASE + 0x2408)
/*#define PDR0                         (REGISTERS_BASE + 0x04ec)*/
/*#define PDR1                         (REGISTERS_BASE + 0x04f0)*/
/*#define PDR2                         (REGISTERS_BASE + 0x04f4)*/
/*#define PDR3                         (REGISTERS_BASE + 0x04f8)*/
/*#define BAR2_ENABLE                  (REGISTERS_BASE + 0x04fc)*/
/*#define BAR2_TRANS                   (REGISTERS_BASE + 0x0500)*/
/*#define BAR2_MASK                    (REGISTERS_BASE + 0x0504)*/
#define PCI_MEM_SIZE1                  (REGISTERS_BASE + 0x2428)
#define PCI_MEM_OFFSET1                (REGISTERS_BASE + 0x242C)
#define PCI_MEM_OFFSET2                (REGISTERS_BASE + 0x2430)
/*#define PCI_IO_SIZE1                 (REGISTERS_BASE + 0x0514)*/
/*#define PCI_IO_OFFSET1               (REGISTERS_BASE + 0x0518)*/
/*#define PCI_IO_OFFSET2               (REGISTERS_BASE + 0x051c)*/
/*#define PCI_CFG_OFFSET               (REGISTERS_BASE + 0x0520)*/
#define PCMCIA_CFG                     (REGISTERS_BASE + 0x2444)
#define PCMCIA_CTL                     (REGISTERS_BASE + 0x2448)
#define PCMCIA_CFG2                    (REGISTERS_BASE + 0x244C) /* new*/
#define SRAM_PAGE                      (REGISTERS_BASE + 0x2450)
#define CFG_PULLUPDN                   (REGISTERS_BASE + 0x2454)
#define CIS_MAP                        (REGISTERS_BASE + 0x2458) /* new*/
#define ENDIAN_CTRL                    (REGISTERS_BASE + 0x245C) /* new*/
#define GS_SLEEP_ACCESS                (REGISTERS_BASE + 0x2480) /* new*/
#define PCMCIA_PWR_DN                  (REGISTERS_BASE + 0x04C4) 
#define PCI_OUTPUT_DLY_CFG             (REGISTERS_BASE + 0x2464) /* new*/

/* VLYNQ registers*/
/* VLYNQ2 was removed from hardware*/
#define VL1_REV_ID                     (REGISTERS_BASE + 0x8000) /* VLYNQ_REVISION*/
#define VL1_CTL                        (REGISTERS_BASE + 0x8004) /* VLYNQ_ CONTROL*/
#define VL1_STS                        (REGISTERS_BASE + 0x8008) /* VLYNQ_STATUS*/
#define VLYNQ_INTVEC                   (REGISTERS_BASE + 0x800C)
#define VL1_INT_STS                    (REGISTERS_BASE + 0x8010) /* VLYNQ_INTCR*/
#define VL1_INT_PEND                   (REGISTERS_BASE + 0x8014) /* VLYNQ_INTSR*/
#define VL1_INT_PTR                    (REGISTERS_BASE + 0x8018) /* VLYNQ_INTPTR*/
#define VL1_TX_ADDR                    (REGISTERS_BASE + 0x801C) /* VLYNQ_TX_MAP_ADDR*/
#define VL1_RX_SIZE1                   (REGISTERS_BASE + 0x8020) /* VLYNQ_RX_MAP_SIZE1*/
#define VL1_RX_OFF1                    (REGISTERS_BASE + 0x8024) /* VLYNQ_RX_MAP_OFFSET1*/
#define VL1_RX_SIZE2                   (REGISTERS_BASE + 0x8028) /* VLYNQ_RX_MAP_SIZE2*/
#define VL1_RX_OFF2                    (REGISTERS_BASE + 0x802C) /* VLYNQ_RX_MAP_OFFSET2*/
#define VL1_RX_SIZE3                   (REGISTERS_BASE + 0x8030) /* VLYNQ_RX_MAP_SIZE3*/
#define VL1_RX_OFF3                    (REGISTERS_BASE + 0x8034) /* VLYNQ_RX_MAP_OFFSET3*/
#define VL1_RX_SIZE4                   (REGISTERS_BASE + 0x8038) /* VLYNQ_RX_MAP_SIZE4*/
#define VL1_RX_OFF4                    (REGISTERS_BASE + 0x803C) /* VLYNQ_RX_MAP_OFFSET4*/
#define VL1_CHIP_VER                   (REGISTERS_BASE + 0x8040) /* VLYNQ_CHIP_VER*/
#define VLYNQ_AUTONEG                  (REGISTERS_BASE + 0x8044)
#define VLYNQ_MANNEG                   (REGISTERS_BASE + 0x8048)
#define VLYNQ_NEGSTAT                  (REGISTERS_BASE + 0x804C)
#define VLYNQ_ENDIAN                   (REGISTERS_BASE + 0x805C)
#define VL1_INT_VEC3_0                 (REGISTERS_BASE + 0x8060) /* VLYNQ_HW_INT3TO0_CFG*/
#define VL1_INT_VEC7_4                 (REGISTERS_BASE + 0x8064) /* VLYNQ_HW_INT7TO4_CFG*/
/* VLYNQ Remote configuration registers*/
#define VL1_REM_REV_ID                 (REGISTERS_BASE + 0x8080) /* VLYNQ_REM_REVISION*/
#define VL1_REM_CTL                    (REGISTERS_BASE + 0x8084) /* VLYNQ_REM_ CONTROL*/
#define VL1_REM_STS                    (REGISTERS_BASE + 0x8088) /* VLYNQ_REM_STATUS*/
#define VLYNQ_REM_INTVEC               (REGISTERS_BASE + 0x808C)
#define VL1_REM_INT_STS                (REGISTERS_BASE + 0x8090) /* VLYNQ_REM_INTCR*/
#define VL1_REM_INT_PEND               (REGISTERS_BASE + 0x8094) /* VLYNQ_REM_INTSR*/
#define VL1_REM_INT_PTR                (REGISTERS_BASE + 0x8098) /* VLYNQ_REM_INTPTR*/
#define VL1_REM_TX_ADDR                (REGISTERS_BASE + 0x809C) /* VLYNQ_REM_TX_MAP_ADDR*/
#define VL1_REM_RX_SIZE1               (REGISTERS_BASE + 0x80A0) /* VLYNQ_REM_RX_MAP_SIZE1*/
#define VL1_REM_RX_OFF1                (REGISTERS_BASE + 0x80A4) /* VLYNQ_REM_RX_MAP_OFFSET1*/
#define VL1_REM_RX_SIZE2               (REGISTERS_BASE + 0x80A8) /* VLYNQ_REM_RX_MAP_SIZE2*/
#define VL1_REM_RX_OFF2                (REGISTERS_BASE + 0x80AC) /* VLYNQ_REM_RX_MAP_OFFSET2*/
#define VL1_REM_RX_SIZE3               (REGISTERS_BASE + 0x80B0) /* VLYNQ_REM_RX_MAP_SIZE3*/
#define VL1_REM_RX_OFF3                (REGISTERS_BASE + 0x80B4) /* VLYNQ_REM_RX_MAP_OFFSET3*/
#define VL1_REM_RX_SIZE4               (REGISTERS_BASE + 0x80B8) /* VLYNQ_REM_RX_MAP_SIZE4*/
#define VL1_REM_RX_OFF4                (REGISTERS_BASE + 0x80BC) /* VLYNQ_REM_RX_MAP_OFFSET4*/
#define VL1_REM_CHIP_VER               (REGISTERS_BASE + 0x80C0) /* VLYNQ_REM_CHIP_VER*/
#define VLYNQ_REM_AUTONEG              (REGISTERS_BASE + 0x80C4)
#define VLYNQ_REM_MANNEG               (REGISTERS_BASE + 0x80C8)
#define VLYNQ_REM_NEGSTAT              (REGISTERS_BASE + 0x80CC)
#define VLYNQ_REM_ENDIAN               (REGISTERS_BASE + 0x80DC)
#define VL1_REM_INT_VEC3_0             (REGISTERS_BASE + 0x80E0) /* VLYNQ_REM_HW_INT3TO0_CFG*/
#define VL1_REM_INT_VEC7_4             (REGISTERS_BASE + 0x80E4) /* VLYNQ_REM_HW_INT7TO4_CFG*/

/* PCIIF*/
/**/
#define PCI_ID_REG                     (REGISTERS_BASE + 0x8400)
#define PCI_STATUS_SET_REG             (REGISTERS_BASE + 0x8410)
#define PCI_STATUS_CLR_REG             (REGISTERS_BASE + 0x8414)
#define PCI_HIMASK_SET_REG             (REGISTERS_BASE + 0x8420)
#define PCI_HIMASK_CLR_REG             (REGISTERS_BASE + 0x8424)
#define PCI_AMASK_SET_REG              (REGISTERS_BASE + 0x8430)
#define PCI_AMASK_CLR_REG              (REGISTERS_BASE + 0x8434)
#define PCI_CLKRUN_REG                 (REGISTERS_BASE + 0x8438)
#define PCI_BE_VENDOR_ID_REG           (REGISTERS_BASE + 0x8500)
#define PCI_BE_COMMAND_REG             (REGISTERS_BASE + 0x8504)
#define PCI_BE_REVISION_REG            (REGISTERS_BASE + 0x8508)
#define PCI_BE_CL_SIZE_REG             (REGISTERS_BASE + 0x850C)
#define PCI_BE_BAR0_MASK_REG           (REGISTERS_BASE + 0x8510)
#define PCI_BE_BAR1_MASK_REG           (REGISTERS_BASE + 0x8514)
#define PCI_BE_BAR2_MASK_REG           (REGISTERS_BASE + 0x8518)
#define PCI_BE_BAR3_MASK_REG           (REGISTERS_BASE + 0x851C)
#define PCI_BE_CIS_PTR_REG             (REGISTERS_BASE + 0x8528)
#define PCI_BE_SUBSYS_ID_REG           (REGISTERS_BASE + 0x852C)
#define PCI_BE_CAP_PTR_REG             (REGISTERS_BASE + 0x8534)
#define PCI_BE_INTR_LINE_REG           (REGISTERS_BASE + 0x853C)
#define PCI_BE_PM_CAP_REG              (REGISTERS_BASE + 0x8540)
#define PCI_BE_PM_CTRL_REG             (REGISTERS_BASE + 0x8544)
#define PCI_BE_PM_D0_CTRL_REG          (REGISTERS_BASE + 0x8560)
#define PCI_BE_PM_D1_CTRL_REG          (REGISTERS_BASE + 0x8564)
#define PCI_BE_PM_D2_CTRL_REG          (REGISTERS_BASE + 0x8568)
#define PCI_BE_PM_D3_CTRL_REG          (REGISTERS_BASE + 0x856C)
#define PCI_BE_SLV_CFG_REG             (REGISTERS_BASE + 0x8580)
#define PCI_BE_ARB_CTRL_REG            (REGISTERS_BASE + 0x8584)
                                       
#define FER                            (REGISTERS_BASE + 0x85A0) /* PCI_BE_STSCHG_FE_REG*/
#define FEMR                           (REGISTERS_BASE + 0x85A4) /* PCI_BE_STSCHG_FEM_REG*/
#define FPSR                           (REGISTERS_BASE + 0x85A8) /* PCI_BE_STSCHG_FPS_REG*/
#define FFER                           (REGISTERS_BASE + 0x85AC) /* PCI_BE_STSCHG_FFE_REG*/

#define PCI_BE_BAR0_TRANS_REG          (REGISTERS_BASE + 0x85C0)
#define PCI_BE_BAR1_TRANS_REG          (REGISTERS_BASE + 0x85C4)
#define PCI_BE_BAR2_TRANS_REG          (REGISTERS_BASE + 0x85C8)
#define PCI_BE_BAR3_TRANS_REG          (REGISTERS_BASE + 0x85CC)
#define PCI_BE_BAR4_TRANS_REG          (REGISTERS_BASE + 0x85D0)
#define PCI_BE_BAR5_TRANS_REG          (REGISTERS_BASE + 0x85D4)
#define PCI_BE_BAR0_REG                (REGISTERS_BASE + 0x85E0)
#define PCI_BE_BAR1_REG                (REGISTERS_BASE + 0x85E4)
#define PCI_BE_BAR2_REG                (REGISTERS_BASE + 0x85E8)
#define PCI_BE_BAR3_REG                (REGISTERS_BASE + 0x85EC)

#define PCI_PROXY_DATA                 (REGISTERS_BASE + 0x8700)
#define PCI_PROXY_ADDR                 (REGISTERS_BASE + 0x8704)
#define PCI_PROXY_CMD                  (REGISTERS_BASE + 0x8708)
#define PCI_CONTROL                    (REGISTERS_BASE + 0x8710)

/* USB1.1 registers*/
/**/
#define USB_STS_CLR                    (REGISTERS_BASE + 0x4000)
#define USB_STS_ND                     (REGISTERS_BASE + 0x4004)
#define USB_INT_ACK                    (REGISTERS_BASE + 0x4008)
#define USB_MASK                       (REGISTERS_BASE + 0x400c)
#define USB_MASK_SET                   (REGISTERS_BASE + 0x4010)
#define USB_MASK_CLR                   (REGISTERS_BASE + 0x4014)
#define USB_WU                         (REGISTERS_BASE + 0x4018)
#define USB_EP0_OUT_PTR                (REGISTERS_BASE + 0x401c)
#define USB_EP0_OUT_VLD                (REGISTERS_BASE + 0x4020)
#define USB_EP0_OUT_LEN                (REGISTERS_BASE + 0x4024)
#define USB_EP0_IN_PTR                 (REGISTERS_BASE + 0x4028)
#define USB_EP0_IN_VLD                 (REGISTERS_BASE + 0x402c)
#define USB_EP0_IN_LEN                 (REGISTERS_BASE + 0x4030)
#define USB_EP1_CFG                    (REGISTERS_BASE + 0x4034)
#define USB_EP1_OUT_INT_CFG            (REGISTERS_BASE + 0x4038)
#define USB_EP1_OUT_PTR                (REGISTERS_BASE + 0x403c)
#define USB_EP1_OUT_VLD                (REGISTERS_BASE + 0x4040)
#define USB_EP1_OUT_CUR_MBLK           (REGISTERS_BASE + 0x4044)
#define USB_EP1_OUT_LEN                (REGISTERS_BASE + 0x4048)
#define USB_EP1_IN_START_MBLK          (REGISTERS_BASE + 0x404c)
#define USB_EP1_IN_LAST_MBLK           (REGISTERS_BASE + 0x4050)
#define USB_EP1_IN_VLD                 (REGISTERS_BASE + 0x4054)

#define USB_EP2_PTR                    (REGISTERS_BASE + 0x405c)
#define USB_EP2_VLD                    (REGISTERS_BASE + 0x4060)
#define USB_EP2_LEN                    (REGISTERS_BASE + 0x4064)
#define USB_EP3_OUT_PTR0               (REGISTERS_BASE + 0x4068)
#define USB_EP3_OUT_VLD0               (REGISTERS_BASE + 0x406c)
#define USB_EP3_OUT_LEN0               (REGISTERS_BASE + 0x4070)
#define USB_EP3_OUT_PTR1               (REGISTERS_BASE + 0x4074)
#define USB_EP3_OUT_VLD1               (REGISTERS_BASE + 0x4078)
#define USB_EP3_OUT_LEN1               (REGISTERS_BASE + 0x407c)
#define USB_EP3_IN_PTR0                (REGISTERS_BASE + 0x4080)
#define USB_EP3_IN_VLD0                (REGISTERS_BASE + 0x4084)
#define USB_EP3_IN_LEN0                (REGISTERS_BASE + 0x4088)
#define USB_EP3_IN_PTR1                (REGISTERS_BASE + 0x408c)
#define USB_EP3_IN_VLD1                (REGISTERS_BASE + 0x4090)
#define USB_EP3_IN_LEN1                (REGISTERS_BASE + 0x4094)
#define USB_EP1_OUT_END_MBLK           (REGISTERS_BASE + 0x4098)
#define USB_EP0_OUT_SETUP              (REGISTERS_BASE + 0x409c)
#define USB_EP0_STALL                  (REGISTERS_BASE + 0x40a0)
#define USB_EP1_IN_OFFSET              (REGISTERS_BASE + 0x40a4)

/* Device Configuration registers*/
#define SOR_CFG                        (REGISTERS_BASE + 0x0800)
#define ECPU_CTRL                      (REGISTERS_BASE + 0x0804)
#define HI_CFG                         (REGISTERS_BASE + 0x0808)
#define EE_START                       (REGISTERS_BASE + 0x080C)

/* IO Control registers*/
#define SERIAL_HOST_IOCFG0             (REGISTERS_BASE + 0x0894) /* new*/
#define SERIAL_HOST_IOCFG1             (REGISTERS_BASE + 0x0898) /* new*/
#define SERIAL_HOST_IOCFG2             (REGISTERS_BASE + 0x089C) /* new*/
#define SERIAL_HOST_IOCFG3             (REGISTERS_BASE + 0x08A0) /* new*/
#define GPIO_IOCFG0                    (REGISTERS_BASE + 0x08F4) /* new*/
#define GPIO_IOCFG1                    (REGISTERS_BASE + 0x08F8) /* new*/
#define GPIO_IOCFG2                    (REGISTERS_BASE + 0x08FC) /* new*/
#define GPIO_IOCFG3                    (REGISTERS_BASE + 0x0900) /* new*/
#define CHIP_ID_B                      (REGISTERS_BASE + 0x5674) /* new*/
#define CHIP_ID                        CHIP_ID_B/* Leave for TNETW compatability*/
#define CHIP_ID_1273_PG10              (0x04030101)
#define CHIP_ID_1273_PG20              (0x04030111)

#define SYSTEM                         (REGISTERS_BASE + 0x0810)
#define PCI_ARB_CFG                    (REGISTERS_BASE + 0x0814)
#define BOOT_IRAM_CFG                  (REGISTERS_BASE + 0x0818)
#define IO_CONTROL_ENABLE              (REGISTERS_BASE + 0x5450)
#define MBLK_CFG                       (REGISTERS_BASE + 0x5460)
#define RS232_BITINTERVAL              (REGISTERS_BASE + 0x0824)
#define TEST_PORT                      (REGISTERS_BASE + 0x096C)
#define DEBUG_PORT                     (REGISTERS_BASE + 0x0970)
#define HOST_WR_ACCESS_REG             (REGISTERS_BASE + 0x09F8)

/* GPIO registers*/
#define GPIO_OE                        (REGISTERS_BASE + 0x082C) /* 22 GPIOs*/
#define GPIO_OUT                       (REGISTERS_BASE + 0x0834)
#define GPIO_IN                        (REGISTERS_BASE + 0x0830)
#define GPO_CFG                        (REGISTERS_BASE + 0x083C)
#define GPIO_SELECT                    (REGISTERS_BASE + 0x614C)
#define GPIO_OE_RADIO                  (REGISTERS_BASE + 0x6140)
#define PWRDN_BUS_L                    (REGISTERS_BASE + 0x0844)
#define PWRDN_BUS_H                    (REGISTERS_BASE + 0x0848)
#define DIE_ID_L                       (REGISTERS_BASE + 0x088C)
#define DIE_ID_H                       (REGISTERS_BASE + 0x0890)

/* Power Management registers*/
/* */
#define ELP_START                      (REGISTERS_BASE + 0x5800)
#define ELP_CFG_MODE                   (REGISTERS_BASE + 0x5804)
#define ELP_CMD                        (REGISTERS_BASE + 0x5808)
#define PLL_CAL_TIME                   (REGISTERS_BASE + 0x5810)
#define CLK_REQ_TIME                   (REGISTERS_BASE + 0x5814)
#define CLK_BUF_TIME                   (REGISTERS_BASE + 0x5818)

#define CFG_PLL_SYNC_CNT               (REGISTERS_BASE + 0x5820) /* Points to the CFG_PLL_SYNC_CNT_xx registers set*/
#define CFG_PLL_SYNC_CNT_I             (REGISTERS_BASE + 0x5820)
#define CFG_PLL_SYNC_CNT_II            (REGISTERS_BASE + 0x5824)
#define CFG_PLL_SYNC_CNT_III           (REGISTERS_BASE + 0x5828)

#define CFG_ELP_SLEEP_CNT              (REGISTERS_BASE + 0x5830) /* Points to the CFG_ELP_SLEEP_CNT_xx registers set*/
#define CFG_ELP_SLEEP_CNT_I            (REGISTERS_BASE + 0x5830)
#define CFG_ELP_SLEEP_CNT_II           (REGISTERS_BASE + 0x5834)
#define CFG_ELP_SLEEP_CNT_III          (REGISTERS_BASE + 0x5838)
#define CFG_ELP_SLEEP_CNT_IV           (REGISTERS_BASE + 0x583c)

#define ELP_SLEEP_CNT                  (REGISTERS_BASE + 0x5840) /* Points to the ELP_SLEEP_CNT_xx registers set*/
#define ELP_SLEEP_CNT_I                (REGISTERS_BASE + 0x5840)
#define ELP_SLEEP_CNT_II               (REGISTERS_BASE + 0x5844)
#define ELP_SLEEP_CNT_III              (REGISTERS_BASE + 0x5848)
#define ELP_SLEEP_CNT_IV               (REGISTERS_BASE + 0x584c)

#define ELP_WAKE_UP_STS                (REGISTERS_BASE + 0x5850)
#define CFG_SLP_CLK_SEL                (REGISTERS_BASE + 0x5860)
#define CFG_SLP_CLK_EN                 (REGISTERS_BASE + 0x5870)

#define CFG_WAKE_UP_EN_I               (REGISTERS_BASE + 0x5880)
#define CFG_WAKE_UP_EN_II              (REGISTERS_BASE + 0x5884)
#define CFG_WAKE_UP_EN_III             (REGISTERS_BASE + 0x5888)

#define CFG_ELP_PWRDN_I                (REGISTERS_BASE + 0x5890)
#define CFG_ELP_PWRDN_II               (REGISTERS_BASE + 0x5894)
#define CFG_ELP_PWRDN_III              (REGISTERS_BASE + 0x5898)

#define CFG_POWER_DOWN_I               (REGISTERS_BASE + 0x58a0)
#define CFG_POWER_DOWN_II              (REGISTERS_BASE + 0x58a4)
#define CFG_POWER_DOWN_III             (REGISTERS_BASE + 0x58a8)

#define CFG_BUCK_TESTMODE_I            (REGISTERS_BASE + 0x58b0)
#define CFG_BUCK_TESTMODE_II           (REGISTERS_BASE + 0x58b4)

#define POWER_STATUS_I                 (REGISTERS_BASE + 0x58C0)
#define POWER_STATUS_II                (REGISTERS_BASE + 0x58C4)

#define DIGLDO_BIAS_PROG_I             (REGISTERS_BASE + 0x58d0)
#define DIGLDO_BIAS_PROG_II            (REGISTERS_BASE + 0x58d4)

#define LDO2P8_BIAS_PROG_I             (REGISTERS_BASE + 0x58e0)
#define LDO2P8_BIAS_PROG_II            (REGISTERS_BASE + 0x58e4)

#define ADCLDO_BIAS_PROG               (REGISTERS_BASE + 0x58f0)

#define REFSYS_PROG_I                  (REGISTERS_BASE + 0x5910)
#define REFSYS_PROG_II                 (REGISTERS_BASE + 0x5914)

#define PM_TEST_I                      (REGISTERS_BASE + 0x5920)
#define PM_TEST_II                     (REGISTERS_BASE + 0x5924)

#define POR_PROG                       (REGISTERS_BASE + 0x5930)

#define TEST_PIN_DIR_I                 (REGISTERS_BASE + 0x5940)
#define TEST_PIN_DIR_II                (REGISTERS_BASE + 0x5944)

#define PROC_CTL                       (REGISTERS_BASE + 0x5950)

#define ADC_REF_WAKEUP_I               (REGISTERS_BASE + 0x5960)
#define ADC_REF_WAKEUP_II              (REGISTERS_BASE + 0x5964)
#define ADC_REF_WAKEUP_III             (REGISTERS_BASE + 0x5968)
#define ADC_REF_WAKEUP_IV              (REGISTERS_BASE + 0x596C)

#define VREG_WAKEUP_I                  (REGISTERS_BASE + 0x5970)
#define VREG_WAKEUP_II                 (REGISTERS_BASE + 0x5974)
#define VREG_WAKEUP_III                (REGISTERS_BASE + 0x5978)
#define VREG_WAKEUP_IV                 (REGISTERS_BASE + 0x597C)

#define PLL_WAKEUP_I                   (REGISTERS_BASE + 0x5980)
#define PLL_WAKEUP_II                  (REGISTERS_BASE + 0x5984)
#define PLL_WAKEUP_III                 (REGISTERS_BASE + 0x5988)
#define PLL_WAKEUP_IV                  (REGISTERS_BASE + 0x598C)

#define XTALOSC_WAKEUP_I               (REGISTERS_BASE + 0x5990)
#define XTALOSC_WAKEUP_II              (REGISTERS_BASE + 0x5994)
#define XTALOSC_WAKEUP_III             (REGISTERS_BASE + 0x5998)
#define XTALOSC_WAKEUP_IV              (REGISTERS_BASE + 0x599C)

/* ----------*/

#define PLL_PARAMETERS                 (REGISTERS_BASE + 0x6040)
#define WU_COUNTER_PAUSE               (REGISTERS_BASE + 0x6008)
#define WELP_ARM_COMMAND               (REGISTERS_BASE + 0x6100)

/* ----------*/

#define POWER_MGMT2                    (REGISTERS_BASE + 0x0840)
#define POWER_MGMT                     (REGISTERS_BASE + 0x5098)
#define MAC_HW_DOZE                    (REGISTERS_BASE + 0x090c)
#define ECPU_SLEEP                     (REGISTERS_BASE + 0x0840)
#define DOZE_CFG                       (REGISTERS_BASE + 0x54bc)
#define DOZE2_CFG                      (REGISTERS_BASE + 0x081c)
#define WAKEUP_CFG                     (REGISTERS_BASE + 0x54c0)
#define WAKEUP_TIME_L                  (REGISTERS_BASE + 0x54c8)
#define WAKEUP_TIME_H                  (REGISTERS_BASE + 0x54c4)

/**/

/*#define CPU_WAIT_CFG                 (f0020)*/
/*#define CFG_QOS_ACM                  (f0046)*/

/* Scratch Pad registers*/
#define SCR_PAD0                       (REGISTERS_BASE + 0x5608)
#define SCR_PAD1                       (REGISTERS_BASE + 0x560C)
#define SCR_PAD2                       (REGISTERS_BASE + 0x5610)
#define SCR_PAD3                       (REGISTERS_BASE + 0x5614)
#define SCR_PAD4                       (REGISTERS_BASE + 0x5618)
#define SCR_PAD4_SET                   (REGISTERS_BASE + 0x561C)
#define SCR_PAD4_CLR                   (REGISTERS_BASE + 0x5620)
#define SCR_PAD5                       (REGISTERS_BASE + 0x5624)
#define SCR_PAD5_SET                   (REGISTERS_BASE + 0x5628)
#define SCR_PAD5_CLR                   (REGISTERS_BASE + 0x562C)
#define SCR_PAD6                       (REGISTERS_BASE + 0x5630)
#define SCR_PAD7                       (REGISTERS_BASE + 0x5634)
#define SCR_PAD8                       (REGISTERS_BASE + 0x5638)
#define SCR_PAD9                       (REGISTERS_BASE + 0x563C)

/* Spare registers*/
#define SPARE_A1                       (REGISTERS_BASE + 0x0994)
#define SPARE_A2                       (REGISTERS_BASE + 0x0998)
#define SPARE_A3                       (REGISTERS_BASE + 0x099C)
#define SPARE_A4                       (REGISTERS_BASE + 0x09A0)
#define SPARE_A5                       (REGISTERS_BASE + 0x09A4)
#define SPARE_A6                       (REGISTERS_BASE + 0x09A8)
#define SPARE_A7                       (REGISTERS_BASE + 0x09AC)
#define SPARE_A8                       (REGISTERS_BASE + 0x09B0)
#define SPARE_B1                       (REGISTERS_BASE + 0x5420)
#define SPARE_B2                       (REGISTERS_BASE + 0x5424)
#define SPARE_B3                       (REGISTERS_BASE + 0x5428)
#define SPARE_B4                       (REGISTERS_BASE + 0x542C)
#define SPARE_B5                       (REGISTERS_BASE + 0x5430)
#define SPARE_B6                       (REGISTERS_BASE + 0x5434)
#define SPARE_B7                       (REGISTERS_BASE + 0x5438)
#define SPARE_B8                       (REGISTERS_BASE + 0x543C)

/* RMAC registers (Raleigh MAC)*/

/* Station registers*/
#define DEV_MODE                       (REGISTERS_BASE + 0x5464)
#define STA_ADDR_L                     (REGISTERS_BASE + 0x546C)
#define STA_ADDR_H                     (REGISTERS_BASE + 0x5470)
#define BSSID_L                        (REGISTERS_BASE + 0x5474)
#define BSSID_H                        (REGISTERS_BASE + 0x5478)
#define AID_CFG                        (REGISTERS_BASE + 0x547C)
#define BASIC_RATE_CFG                 (REGISTERS_BASE + 0x4C6C)
#define BASIC_RATE_TX_CFG              (REGISTERS_BASE + 0x55F0)

/* Protocol timers registers*/
#define IFS_CFG0                       (REGISTERS_BASE + 0x5494)
#define IFS_CFG1                       (REGISTERS_BASE + 0x5498)
#define TIMEOUT_CFG                    (REGISTERS_BASE + 0x549C)
#define CONT_WIND_CFG                  (REGISTERS_BASE + 0x54A0)
#define BCN_INT_CFG                    (REGISTERS_BASE + 0x54A4)
#define RETRY_CFG                      (REGISTERS_BASE + 0x54A8)
#define DELAY_CFG                      (REGISTERS_BASE + 0x54B0)

/* Hardware Override registers*/
#define CCA_CFG                        (REGISTERS_BASE + 0x54CC)
#define CCA_FILTER_CFG                 (REGISTERS_BASE + 0x5480)
#define RADIO_PLL_CFG                  (REGISTERS_BASE + 0x555C)
#define CCA_MON                        (REGISTERS_BASE + 0x54D0)
#define TX_FRM_CTL                     (REGISTERS_BASE + 0x54D4)
#define CONT_TX_EN                     (REGISTERS_BASE + 0x50EC)
#define PHY_STANDBY_EN                 (REGISTERS_BASE + 0x5668)

/* Transmit Setup registers*/
#define TX_PING_PONG                   (REGISTERS_BASE + 0x5090)
#define TX_CFG0                        (REGISTERS_BASE + 0x5000)
#define TX_CFG1                        (REGISTERS_BASE + 0x5004)
#define TX_CFG2                        (REGISTERS_BASE + 0x5008)
#define MAX_LIFETIME                   (REGISTERS_BASE + 0x50FC)
#define TX_PANG_SEL                    (REGISTERS_BASE + 0x50E0)
#define TX_PANG0                       (REGISTERS_BASE + 0x50A0)
#define TX_PING0                       (REGISTERS_BASE + 0x5010)
#define TX_PONG0                       (REGISTERS_BASE + 0x5050)
#define TX_PANG1                       (REGISTERS_BASE + 0x50A4)
#define TX_PING1                       (REGISTERS_BASE + 0x5014)
#define TX_PONG1                       (REGISTERS_BASE + 0x5054)
#define TX_PANG2                       (REGISTERS_BASE + 0x50A8)
#define TX_PING2                       (REGISTERS_BASE + 0x5018)
#define TX_PONG2                       (REGISTERS_BASE + 0x5058)
#define TX_PANG3                       (REGISTERS_BASE + 0x50AC)
#define TX_PING3                       (REGISTERS_BASE + 0x501C)
#define TX_PONG3                       (REGISTERS_BASE + 0x505C)
#define TX_PANG4                       (REGISTERS_BASE + 0x50B0)
#define TX_PING4                       (REGISTERS_BASE + 0x5020)
#define TX_PONG4                       (REGISTERS_BASE + 0x5060)
#define TX_PANG5                       (REGISTERS_BASE + 0x50B4)
#define TX_PING5                       (REGISTERS_BASE + 0x5024)
#define TX_PONG5                       (REGISTERS_BASE + 0x5064)
#define TX_PANG6                       (REGISTERS_BASE + 0x50B8)
#define TX_PING6                       (REGISTERS_BASE + 0x5028)
#define TX_PONG6                       (REGISTERS_BASE + 0x5068)
#define TX_PANG7                       (REGISTERS_BASE + 0x50BC)
#define TX_PING7                       (REGISTERS_BASE + 0x502C)
#define TX_PONG7                       (REGISTERS_BASE + 0x506C)
#define TX_PANG8                       (REGISTERS_BASE + 0x50C0)
#define TX_PING8                       (REGISTERS_BASE + 0x5030)
#define TX_PONG8                       (REGISTERS_BASE + 0x5070)
#define TX_PANG9                       (REGISTERS_BASE + 0x50C4)
#define TX_PING9                       (REGISTERS_BASE + 0x5034)
#define TX_PONG9                       (REGISTERS_BASE + 0x5074)
#define TX_PANG10                      (REGISTERS_BASE + 0x50C8)
#define TX_PING10                      (REGISTERS_BASE + 0x5038)
#define TX_PONG10                      (REGISTERS_BASE + 0x5078)
#define TX_PANG11                      (REGISTERS_BASE + 0x50CC)
#define TX_PING11                      (REGISTERS_BASE + 0x503C)
#define TX_PONG11                      (REGISTERS_BASE + 0x507C)

/* Transmit Status registers*/
#define TX_STATUS                      (REGISTERS_BASE + 0x509C)
#define TX_PANG_EXCH                   (REGISTERS_BASE + 0x50D0)
#define TX_PING_EXCH                   (REGISTERS_BASE + 0x5040)
#define TX_PONG_EXCH                   (REGISTERS_BASE + 0x5080)
#define TX_PANG_ATT                    (REGISTERS_BASE + 0x50D4)
#define TX_PING_ATT                    (REGISTERS_BASE + 0x5044)
#define TX_PONG_ATT                    (REGISTERS_BASE + 0x5084)
#define TX_PANG_TIMESTAMP              (REGISTERS_BASE + 0x50DC)
#define TX_PING_TIMESTAMP              (REGISTERS_BASE + 0x504C)
#define TX_PONG_TIMESTAMP              (REGISTERS_BASE + 0x508C)

/* Transmit State registers*/
#define TX_STATE                       (REGISTERS_BASE + 0x5094)
#define TX_PANG_OVRD_CFG               (REGISTERS_BASE + 0x50D8)
#define TX_PING_OVRD_CFG               (REGISTERS_BASE + 0x5048)
#define TX_PONG_OVRD_CFG               (REGISTERS_BASE + 0x5088)
#define TX_HOLD_CFG                    (REGISTERS_BASE + 0x54D8)
#define TSF_ADJ_CFG1                   (REGISTERS_BASE + 0x54DC)
#define TSF_ADJ_CFG2                   (REGISTERS_BASE + 0x54E0)
#define TSF_ADJ_CFG3                   (REGISTERS_BASE + 0x54E4)
#define TSF_ADJ_CFG4                   (REGISTERS_BASE + 0x54E8)
#define CFG_OFDM_TIMES0                (REGISTERS_BASE + 0x5648)
#define CFG_OFDM_TIMES1                (REGISTERS_BASE + 0x564C)

/* Beacon/Probe Response registers*/
#define PRB_ADDR                       (REGISTERS_BASE + 0x54EC)
#define PRB_LENGTH                     (REGISTERS_BASE + 0x54F0)
#define BCN_ADDR                       (REGISTERS_BASE + 0x54F4)
#define BCN_LENGTH                     (REGISTERS_BASE + 0x54F8)
#define TIM_VALID0                     (REGISTERS_BASE + 0x54FC)
#define TIM_ADDR0                      (REGISTERS_BASE + 0x5500)
#define TIM_LENGTH0                    (REGISTERS_BASE + 0x5504)
#define TIM_VALID1                     (REGISTERS_BASE + 0x5654)
#define TIM_ADDR1                      (REGISTERS_BASE + 0x5658)
#define TIM_LENGTH1                    (REGISTERS_BASE + 0x565C)
#define TIM_SELECT                     (REGISTERS_BASE + 0x5660)
#define TSF_CFG                        (REGISTERS_BASE + 0x5508)

/* Other Hardware Generated Frames regi*/
#define CTL_FRM_CFG                    (REGISTERS_BASE + 0x550C)
#define MGMT_FRM_CFG                   (REGISTERS_BASE + 0x5510)
#define CFG_ANT_SEL                    (REGISTERS_BASE + 0x5664)
#define RMAC_ADDR_BASE                 (REGISTERS_BASE + 0x5680) /* new*/

/* Protocol Interface Read Write Interf*/
#define TXSIFS_TIMER                   (REGISTERS_BASE + 0x4C00)
#define TXPIFS_TIMER                   (REGISTERS_BASE + 0x4C04)
#define TXDIFS_TIMER                   (REGISTERS_BASE + 0x4C08)
#define SLOT_TIMER                     (REGISTERS_BASE + 0x4C0C)
#define BACKOFF_TIMER                  (REGISTERS_BASE + 0x4C10)
#define BCN_PSP_TIMER                  (REGISTERS_BASE + 0x4C14)
#define NAV                            (REGISTERS_BASE + 0x4C18)
#define TSF_L                          (REGISTERS_BASE + 0x4C1C)
#define TSF_H                          (REGISTERS_BASE + 0x4C20)
#define TSF_PREV_L                     (REGISTERS_BASE + 0x4CC4) /* new */
#define TSF_PREV_H                     (REGISTERS_BASE + 0x4CC8) /* new */
#define TOUT_TIMER                     (REGISTERS_BASE + 0x4C2C)
#define NEXT_TBTT_L                    (REGISTERS_BASE + 0x4C30)
#define NEXT_TBTT_H                    (REGISTERS_BASE + 0x4C34)
#define DTIM_CNT                       (REGISTERS_BASE + 0x4C38)
#define CONT_WIND                      (REGISTERS_BASE + 0x4C3C)
#define PRSP_REQ                       (REGISTERS_BASE + 0x4C40)
#define PRSP_DA_L                      (REGISTERS_BASE + 0x4C44)
#define PRSP_DA_H                      (REGISTERS_BASE + 0x4C48)
#define PRSP_RETRY                     (REGISTERS_BASE + 0x4C4C)
#define PSPOLL_REQ                     (REGISTERS_BASE + 0x4C50)
#define NEXT_SEQ_NUM                   (REGISTERS_BASE + 0x4C54)
#define PRSP_SEQ_NUM                   (REGISTERS_BASE + 0x4C58)
#define BCN_SEQ_NUM                    (REGISTERS_BASE + 0x4C5C)
#define MED_USAGE                      (REGISTERS_BASE + 0x4C24)
#define MED_USAGE_TM                   (REGISTERS_BASE + 0x4C28)
#define PRB_DLY                        (REGISTERS_BASE + 0x4C60)
#define STA_SRC                        (REGISTERS_BASE + 0x4C64)
#define STA_LRC                        (REGISTERS_BASE + 0x4C68)
#define CFG_ACM                        (REGISTERS_BASE + 0x4C70)
#define RAND_NUMB                      (REGISTERS_BASE + 0x4C6C)
#define CFG_ACK_CTS_DOT11A             (REGISTERS_BASE + 0x4C74)
#define CFG_ACK_CTS_DOT11B             (REGISTERS_BASE + 0x4C78)
#define ACM_IFS_CFG0                   (REGISTERS_BASE + 0x4C7C)
#define ACM_IFS_CFG1                   (REGISTERS_BASE + 0x4C80)
#define ACM_IFS_CFG2                   (REGISTERS_BASE + 0x4C84)
#define ACM_IFS_CFG3                   (REGISTERS_BASE + 0x4C88)
#define ACK_CTS_FRM_CFG                (REGISTERS_BASE + 0x4C8C)
#define CFG_RX_TSTMP_DLY0              (REGISTERS_BASE + 0x4C90)
#define CFG_RX_TSTMP_DLY1              (REGISTERS_BASE + 0x4C94)
#define CFG_RX_TSTMP_DLY2              (REGISTERS_BASE + 0x4C98)
#define CFG_RX_TSTMP_DLY3              (REGISTERS_BASE + 0x4C9C)
#define CCA_BUSY                       (REGISTERS_BASE + 0x4CA0)
#define CCA_BUSY_CLR                   (REGISTERS_BASE + 0x4CA4)
#define CCA_IDLE                       (REGISTERS_BASE + 0x4CA8)
#define CCA_IDLE_CLR                   (REGISTERS_BASE + 0x4CAC)

/* Receive Manager registers*/
#define RX_HEAD_PTR                    (REGISTERS_BASE + 0x567C) /* new*/
#define RX_TAIL_PTR                    (REGISTERS_BASE + 0x4898) /* new*/
#define RX_CURR_PTR                    (REGISTERS_BASE + 0x5678) /* new*/
#define RX_RESET                       (REGISTERS_BASE + 0x4800)
#define RX_MODMODE                     (REGISTERS_BASE + 0x4838) /* new*/
#define MAC_HEADER_BYTECNT             (REGISTERS_BASE + 0x4890)
#define RX_MAC_BYTECNT_INT             (REGISTERS_BASE + 0x489C)
#define MAC_HEADER_WORD0               (REGISTERS_BASE + 0x4868)
#define MAC_HEADER_WORD1               (REGISTERS_BASE + 0x486C)
#define MAC_HEADER_WORD2               (REGISTERS_BASE + 0x4870)
#define MAC_HEADER_WORD3               (REGISTERS_BASE + 0x4874)
#define MAC_HEADER_WORD4               (REGISTERS_BASE + 0x4878)
#define MAC_HEADER_WORD5               (REGISTERS_BASE + 0x487C)
#define MAC_HEADER_WORD6               (REGISTERS_BASE + 0x4880)
#define MAC_HEADER_WORD7               (REGISTERS_BASE + 0x4884)
#define MAC_HEADER_WORD8               (REGISTERS_BASE + 0x4888)
#define MAC_HEADER_WORD9               (REGISTERS_BASE + 0x488C)
#define RX_CFG                         (REGISTERS_BASE + 0x5514)
#define RX_FILTER_CFG                  (REGISTERS_BASE + 0x55B4)
#define RX_MC0_L                       (REGISTERS_BASE + 0x5518)
#define RX_MC0_H                       (REGISTERS_BASE + 0x551C)
#define RX_MC1_L                       (REGISTERS_BASE + 0x5520)
#define RX_MC1_H                       (REGISTERS_BASE + 0x5524)
#define STA_SSID0                      (REGISTERS_BASE + 0x4804)
#define STA_SSID1                      (REGISTERS_BASE + 0x4808)
#define STA_SSID2                      (REGISTERS_BASE + 0x480C)
#define STA_SSID3                      (REGISTERS_BASE + 0x4810)
#define STA_SSID4                      (REGISTERS_BASE + 0x4814)
#define STA_SSID5                      (REGISTERS_BASE + 0x4818)
#define STA_SSID6                      (REGISTERS_BASE + 0x481C)
#define STA_SSID7                      (REGISTERS_BASE + 0x4820)
#define SSID_LEN                       (REGISTERS_BASE + 0x4824)
#define RX_FREE_MEM                    (REGISTERS_BASE + 0x5528)
#define RX_CURR_MEM                    (REGISTERS_BASE + 0x552C)
#define MAC_TIMESTAMP                  (REGISTERS_BASE + 0x5560) /* Check place*/
#define RX_TIMESTAMP                   (REGISTERS_BASE + 0x5564)
#define RX_FRM_PTR                     (REGISTERS_BASE + 0x5568)
#define RX_FRM_LEN                     (REGISTERS_BASE + 0x556C)
#define RX_PLCP_HDR                    (REGISTERS_BASE + 0x5570)
#define RX_PLCP_SIGNAL                 (REGISTERS_BASE + 0x5574)
#define RX_PLCP_SERVICE                (REGISTERS_BASE + 0x5578) /* 16 bits ?*/
#define RX_PLCP_LENGTH                 (REGISTERS_BASE + 0x557C)
#define RX_FRM_CTL                     (REGISTERS_BASE + 0x5580)
#define RX_DUR_ID                      (REGISTERS_BASE + 0x5584)
#define RX_ADDR1_L                     (REGISTERS_BASE + 0x5588)
#define RX_ADDR1_H                     (REGISTERS_BASE + 0x558C)
#define RX_ADDR2_L                     (REGISTERS_BASE + 0x5590)
#define RX_ADDR2_H                     (REGISTERS_BASE + 0x5594)
#define RX_ADDR3_L                     (REGISTERS_BASE + 0x5598)
#define RX_ADDR3_H                     (REGISTERS_BASE + 0x559C)
#define RX_SEQ_CTL                     (REGISTERS_BASE + 0x55A0)
#define RX_WEP_IV                      (REGISTERS_BASE + 0x55A4)
#define RX_TIME_L                      (REGISTERS_BASE + 0x55A8)
#define RX_TIME_H                      (REGISTERS_BASE + 0x55AC)
#define RX_STATUS                      (REGISTERS_BASE + 0x55B0)
#define PLCP_ERR_CNT                   (REGISTERS_BASE + 0x4828)
#define FCS_ERR_CNT                    (REGISTERS_BASE + 0x482C)
#define RX_OVERFLOW_CNT                (REGISTERS_BASE + 0x4830)
#define RX_DEBUG1                      (REGISTERS_BASE + 0x4858)
#define RX_DEBUG2                      (REGISTERS_BASE + 0x485C)
#define RX_QOS_CFG                     (REGISTERS_BASE + 0x4848)
#define RX_QOS_CTL                     (REGISTERS_BASE + 0x4844)
#define RX_QOS_STATUS                  (REGISTERS_BASE + 0x4854) /* new name RX_QOS_STS*/
#define RX_TXOP_HOLDER_L               (REGISTERS_BASE + 0x484C)
#define RX_TXOP_HOLDER_H               (REGISTERS_BASE + 0x4850)
#define RX_FRM_CNT                     (REGISTERS_BASE + 0x4834) /* what is RX_FRM_CTR*/
#define CONS_FCS_ERR_CNT               (REGISTERS_BASE + 0x483C)
#define CONS_FCS_ERR_CFG               (REGISTERS_BASE + 0x4840)
#define RX_QOS_CTL_MASK                (REGISTERS_BASE + 0x48A0) /* new*/
#define RX_QOS_ACK_EN                  (REGISTERS_BASE + 0x48A4) /* new*/
#define RX_QOS_NOACK_EN                (REGISTERS_BASE + 0x48A8) /* new*/
#define RX_QOS_ACK_BITMAP              (REGISTERS_BASE + 0x48AC) /* new*/

/* Baseband Processor registers*/
#define SBB_CFG                        (REGISTERS_BASE + 0x55C8)
#define SBB_ADDR                       (REGISTERS_BASE + 0x55D0)
#define SBB_DATA                       (REGISTERS_BASE + 0x55D4)
#define SBB_CTL                        (REGISTERS_BASE + 0x55D8)

/* Radio Control Interface registers*/
#define RCI_CTL                        (REGISTERS_BASE + 0x55DC)
#define RCI_DATA                       (REGISTERS_BASE + 0x55E0)
#define RCI_CFG1                       (REGISTERS_BASE + 0x55E4)
#define RCI_CFG2                       (REGISTERS_BASE + 0x55E8)
#define RCI_CFG3                       (REGISTERS_BASE + 0x55EC)

#define TNET1150_LAST_REG_ADDR			PCI_CONTROL

#define ECPU_CONTROL_HALT			   0x00000101				

/*0x03bc00 address is 1KB from end of FW RAM in 125x chip*/
#define FW_STATIC_NVS_TRAGET_ADDRESS   0x03bc00

/* Command mail box address */
#define CMD_MBOX_ADDRESS               0x407B4

/* Top Register */
#define INDIRECT_REG1                  (REGISTERS_BASE + 0x9B0)
#define OCP_POR_CTR                    (REGISTERS_BASE + 0x9B4)
#define OCP_POR_WDATA                  (REGISTERS_BASE + 0x9B8)
#define OCP_DATA_RD                    (REGISTERS_BASE + 0x9BC)
#define OCP_CMD                        (REGISTERS_BASE + 0x9C0)
#define FUNC7_SEL                      0xC8C
#define FUNC7_PULL                     0xCB0
#define FN0_CCCR_REG_32                0x64  

#define PLL_PARAMETERS_CLK_VAL_19_2M   0x01			
#define PLL_PARAMETERS_CLK_VAL_26M     0x02				
#define PLL_PARAMETERS_CLK_VAL_38_4M   0x03				
#define PLL_PARAMETERS_CLK_VAL_52M     0x04				

#define WU_COUNTER_PAUSE_VAL           0x3FF

/* Base band clocker register */
#define WELP_ARM_COMMAND_VAL           0x4

/* Command mail box address */
#define CMD_MBOX_ADDRESS               0x407B4

#endif
