#ifndef __PANEL_LG4591_H__
#define __PANEL_LG4591_H__

/* Register addresses */
#define DISPLAY_CTRL1	0xb5
#define DISPLAY_CTRL2	0xb6
#define OSCSET		0xc0
#define PWRCTL1		0xc1
#define PWRCTL2		0xc2
#define PWRCTL3		0xc3
#define PWRCTL4		0xc4
#define RGAMMAP		0xd0
#define RGAMMAN		0xd1
#define GGAMMAP		0xd2
#define GGAMMAN		0xd3
#define BGAMMAP		0xd4
#define BGAMMAN		0xd5
#define DSI_CFG		0xe0
#define OTP2		0xf9

/* Commands */
#define WRDISBV		0x51
#define RDDISBV		0x52
#define WRCTRLD		0x53
#define RDCTRLD		0x54

#endif
