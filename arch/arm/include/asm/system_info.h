#ifndef __ASM_ARM_SYSTEM_INFO_H
#define __ASM_ARM_SYSTEM_INFO_H

#define CPU_ARCH_UNKNOWN	0
#define CPU_ARCH_ARMv3		1
#define CPU_ARCH_ARMv4		2
#define CPU_ARCH_ARMv4T		3
#define CPU_ARCH_ARMv5		4
#define CPU_ARCH_ARMv5T		5
#define CPU_ARCH_ARMv5TE	6
#define CPU_ARCH_ARMv5TEJ	7
#define CPU_ARCH_ARMv6		8
#define CPU_ARCH_ARMv7		9

#ifndef __ASSEMBLY__

/* information about the system we're running on */
extern unsigned int system_rev;
extern unsigned int system_serial_low;
extern unsigned int system_serial_high;
extern unsigned int mem_fclk_21285;

#if defined(CONFIG_MACH_OMAP4_BOWSER)
#define SERIAL16_SIZE	16
#define REVISION16_SIZE	16
#define MAC_ADDR_SIZE	16
#define MAC_SEC_SIZE	32
#define BOOTMODE_SIZE	16
#define PRODUCTID_SIZE	32

extern unsigned char system_rev16[REVISION16_SIZE+1];
extern unsigned char system_serial16[SERIAL16_SIZE+1];
extern unsigned char system_mac_addr[MAC_ADDR_SIZE+1];
extern unsigned char system_mac_sec[MAC_SEC_SIZE+1];
extern unsigned char system_bootmode[BOOTMODE_SIZE+1];
extern unsigned char system_postmode[BOOTMODE_SIZE+1];
extern unsigned char system_bt_mac_addr[MAC_ADDR_SIZE+1];
extern unsigned char system_productid[PRODUCTID_SIZE+1];

#endif //CONFIG_MACH_OMAP4_BOWSER
#define GYROCAL_SIZE	36
extern unsigned char system_gyro_cal[GYROCAL_SIZE+1];

extern int __pure cpu_architecture(void);

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_ARM_SYSTEM_INFO_H */
