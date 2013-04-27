
#ifndef __MFD_AIC31XX_REGISTERS_H__
#define __MFD_AIC31XX_REGISTERS_H__

#define MAKE_REG(book, page, offset) \
			(unsigned int)((book << 16)|(page << 8)|offset)

/* ****************** Book 0 Registers **************************************/
/* ****************** AIC31XX has one book only *****************************/

/* ****************** Page 0 Registers **************************************/
/* Page select register */
#define AIC31XX_PAGE_SEL_REG		MAKE_REG(0, 0, 0)
/* Software reset register */
#define AIC31XX_RESET_REG		MAKE_REG(0, 0, 1)
/* OT FLAG register */
#define AIC31XX_OT_FLAG_REG		MAKE_REG(0, 0, 3)
/* Revision and PG-ID */
#define AIC31XX_REV_PG_ID		MAKE_REG(0, 0, 3)
#define AIC31XX_REV_MASK		(0b01110000)
#define AIC31XX_REV_SHIFT		4
#define AIC31XX_PG_MASK			(0b00000001)
#define AIC31XX_PG_SHIFT		0

/* Clock clock Gen muxing, Multiplexers*/
#define AIC31XX_CLK_R1			MAKE_REG(0, 0, 4)
#define AIC31XX_PLL_CLKIN_MASK		(0b00001100)
#define AIC31XX_PLL_CLKIN_SHIFT		2
#define AIC31XX_PLL_CLKIN_MCLK		0
/* PLL P and R-VAL register*/
#define AIC31XX_CLK_R2			MAKE_REG(0, 0, 5)
/* PLL J-VAL register*/
#define AIC31XX_CLK_R3			MAKE_REG(0, 0, 6)
/* PLL D-VAL MSB register */
#define AIC31XX_CLK_R4			MAKE_REG(0, 0, 7)
/* PLL D-VAL LSB register */
#define AIC31XX_CLK_R5			MAKE_REG(0, 0, 8)
/* DAC NDAC_VAL register*/
#define AIC31XX_NDAC_CLK_R6		MAKE_REG(0, 0, 11)
/* DAC MDAC_VAL register */
#define AIC31XX_MDAC_CLK_R7		MAKE_REG(0, 0, 12)
/*DAC OSR setting register1, MSB value*/
#define AIC31XX_DAC_OSR_MSB		MAKE_REG(0, 0, 13)
/*DAC OSR setting register 2, LSB value*/
#define AIC31XX_DAC_OSR_LSB		MAKE_REG(0, 0, 14)
#define AIC31XX_MINI_DSP_INPOL		MAKE_REG(0, 0, 16)
/*Clock setting register 8, PLL*/
#define AIC31XX_NADC_CLK_R8		MAKE_REG(0, 0, 18)
/*Clock setting register 9, PLL*/
#define AIC31XX_MADC_CLK_R9		MAKE_REG(0, 0, 19)
/*ADC Oversampling (AOSR) Register*/
#define AIC31XX_ADC_OSR_REG		MAKE_REG(0, 0, 20)
/*ADC minidsp engine decimation*/
#define AIC31XX_ADC_DSP_DECI		MAKE_REG(0, 0, 22)
/*Clock setting register 9, Multiplexers*/
#define AIC31XX_CLK_MUX_R9		MAKE_REG(0, 0, 25)
/*Clock setting register 10, CLOCKOUT M divider value*/
#define AIC31XX_CLK_R10			MAKE_REG(0, 0, 26)
/*Audio Interface Setting Register 1*/
#define AIC31XX_INTERFACE_SET_REG_1	MAKE_REG(0, 0, 27)
/*Audio Data Slot Offset Programming*/
#define AIC31XX_DATA_SLOT_OFFSET	MAKE_REG(0, 0, 28)
/*Audio Interface Setting Register 2*/
#define AIC31XX_INTERFACE_SET_REG_2	MAKE_REG(0, 0, 29)
/*Clock setting register 11, BCLK N Divider*/
#define AIC31XX_CLK_R11			MAKE_REG(0, 0, 30)
/*Audio Interface Setting Register 3, Secondary Audio Interface*/
#define AIC31XX_INTERFACE_SET_REG_3	MAKE_REG(0, 0, 31)
/*Audio Interface Setting Register 4*/
#define AIC31XX_INTERFACE_SET_REG_4	MAKE_REG(0, 0, 32)
/*Audio Interface Setting Register 5*/
#define AIC31XX_INTERFACE_SET_REG_5	MAKE_REG(0, 0, 33)
/* I2C Bus Condition */
#define AIC31XX_I2C_FLAG_REG		MAKE_REG(0, 0, 34)
/* ADC FLAG */
#define AIC31XX_ADC_FLAG		MAKE_REG(0, 0, 36)
/* DAC Flag Registers */
#define AIC31XX_DAC_FLAG_1		MAKE_REG(0, 0, 37)
#define AIC31XX_DAC_FLAG_2		MAKE_REG(0, 0, 38)

/* Sticky Interrupt flag (overflow) */
#define AIC31XX_OVERFLOW_FLAG_REG	MAKE_REG(0, 0, 39)
#define AIC31XX_LEFT_DAC_OVERFLOW_INT			0x80
#define AIC31XX_RIGHT_DAC_OVERFLOW_INT			0x40
#define AIC31XX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT	0x20
#define AIC31XX_ADC_OVERFLOW_INT			0x08
#define AIC31XX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT	0x02

/* Sticky Interrupt flags 1 and 2 registers (DAC) */
#define AIC31XX_INTR_DAC_FLAG_1		MAKE_REG(0, 0, 44)
#define AIC31XX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT	0x80
#define AIC31XX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT	0x40
#define AIC31XX_BUTTON_PRESS_INT			0x20
#define AIC31XX_HEADSET_PLUG_UNPLUG_INT			0x10
#define AIC31XX_LEFT_DRC_THRES_INT			0x08
#define AIC31XX_RIGHT_DRC_THRES_INT			0x04
#define AIC31XX_MINIDSP_D_STD_INT			0x02
#define AIC31XX_MINIDSP_D_AUX_INT			0x01

#define AIC31XX_INTR_DAC_FLAG_2		MAKE_REG(0, 0, 46)

/* Sticky Interrupt flags 1 and 2 registers (ADC) */
#define AIC31XX_INTR_ADC_FLAG_1		MAKE_REG(0, 0, 45)
#define AIC31XX_AGC_NOISE_INT		0x40
#define AIC31XX_MINIDSP_A_STD_INT	0x10
#define AIC31XX_MINIDSP_A_AUX_INT	0x08

#define AIC31XX_INTR_ADC_FLAG_2		MAKE_REG(0, 0, 47)

/* INT1 interrupt control */
#define AIC31XX_INT1_CTRL_REG		MAKE_REG(0, 0, 48)
#define AIC31XX_HEADSET_IN_MASK		0x80
#define AIC31XX_BUTTON_PRESS_MASK	0x40
#define AIC31XX_DAC_DRC_THRES_MASK	0x20
#define AIC31XX_AGC_NOISE_MASK		0x10
#define AIC31XX_OVER_CURRENT_MASK	0x08
#define AIC31XX_ENGINE_GEN__MASK	0x04

/* INT2 interrupt control */
#define AIC31XX_INT2_CTRL_REG		MAKE_REG(0, 0, 49)

/* GPIO1 control */
#define AIC31XX_GPIO1_IO_CTRL		MAKE_REG(0, 0, 51)
#define AIC31XX_GPIO_D5_D2		(0b00111100)
#define AIC31XX_GPIO_D2_SHIFT		2

/*DOUT Function Control*/
#define AIC31XX_DOUT_CTRL_REG		MAKE_REG(0, 0, 53)
/*DIN Function Control*/
#define AIC31XX_DIN_CTL_REG		MAKE_REG(0, 0, 54)
/*DAC Instruction Set Register*/
#define AIC31XX_DAC_PRB			MAKE_REG(0, 0, 60)
/*ADC Instruction Set Register*/
#define AIC31XX_ADC_PRB			MAKE_REG(0, 0, 61)
/*DAC channel setup register*/
#define AIC31XX_DAC_CHN_REG		MAKE_REG(0, 0, 63)
/*DAC Mute and volume control register*/
#define AIC31XX_DAC_MUTE_CTRL_REG	MAKE_REG(0, 0, 64)
/*Left DAC channel digital volume control*/
#define AIC31XX_LDAC_VOL_REG		MAKE_REG(0, 0, 65)
/*Right DAC channel digital volume control*/
#define AIC31XX_RDAC_VOL_REG		MAKE_REG(0, 0, 66)
/* Headset detection */
#define AIC31XX_HS_DETECT_REG		MAKE_REG(0, 0, 67)
#define AIC31XX_HS_MASK			(0b01100000)
#define AIC31XX_HP_MASK			(0b00100000)
/* DRC Control Registers */
#define AIC31XX_DRC_CTRL_REG_1		MAKE_REG(0, 0, 68)
#define AIC31XX_DRC_CTRL_REG_2		MAKE_REG(0, 0, 69)
#define AIC31XX_DRC_CTRL_REG_3		MAKE_REG(0, 0, 70)
/* Beep Generator */
#define AIC31XX_BEEP_GEN_L		MAKE_REG(0, 0, 71)
#define AIC31XX_BEEP_GEN_R		MAKE_REG(0, 0, 72)
/* Beep Length */
#define AIC31XX_BEEP_LEN_MSB_REG	MAKE_REG(0, 0, 73)
#define AIC31XX_BEEP_LEN_MID_REG	MAKE_REG(0, 0, 74)
#define AIC31XX_BEEP_LEN_LSB_REG	MAKE_REG(0, 0, 75)
/* Beep Functions */
#define AIC31XX_BEEP_SINX_MSB_REG	MAKE_REG(0, 0, 76)
#define AIC31XX_BEEP_SINX_LSB_REG	MAKE_REG(0, 0, 77)
#define AIC31XX_BEEP_COSX_MSB_REG	MAKE_REG(0, 0, 78)
#define AIC31XX_BEEP_COSX_LSB_REG	MAKE_REG(0, 0, 79)
#define AIC31XX_ADC_CHN_REG		MAKE_REG(0, 0, 81)
#define AIC31XX_ADC_VOL_FGC		MAKE_REG(0, 0, 82)
#define AIC31XX_ADC_VOL_CGC		MAKE_REG(0, 0, 83)

/*Channel AGC Control Register 1*/
#define AIC31XX_CHN_AGC_R1		MAKE_REG(0, 0, 86)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R2		MAKE_REG(0, 0, 87)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R3		MAKE_REG(0, 0, 88)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R4		MAKE_REG(0, 0, 89)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R5		MAKE_REG(0, 0, 90)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R6		MAKE_REG(0, 0, 91)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R7		MAKE_REG(0, 0, 92)
/* VOL/MICDET-Pin SAR ADC Volume Control */
#define AIC31XX_VOL_MICDECT_ADC		MAKE_REG(0, 0, 116)
/* VOL/MICDET-Pin Gain*/
#define AIC31XX_VOL_MICDECT_GAIN	MAKE_REG(0, 0, 117)
/*Power Masks*/
#define AIC31XX_LDAC_POWER_STATUS_MASK		0x80
#define AIC31XX_RDAC_POWER_STATUS_MASK		0x08
#define AIC31XX_ADC_POWER_STATUS_MASK		0x40
/*Time Delay*/
#define AIC31XX_TIME_DELAY		5000
#define AIC31XX_DELAY_COUNTER		100



/******************** Page 1 Registers **************************************/
/* Headphone drivers */
#define AIC31XX_HPHONE_DRIVERS		MAKE_REG(0, 1, 31)
/* Class-D Speakear Amplifier */
#define AIC31XX_CLASS_D_SPK		MAKE_REG(0, 1, 32)
/* HP Output Drivers POP Removal Settings */
#define AIC31XX_HP_OUT_DRIVERS		MAKE_REG(0, 1, 33)
/* Output Driver PGA Ramp-Down Period Control */
#define AIC31XX_PGA_RAMP_REG		MAKE_REG(0, 1, 34)
/* DAC_L and DAC_R Output Mixer Routing */
#define AIC31XX_DAC_MIXER_ROUTING	MAKE_REG(0, 1, 35)
/*Left Analog Vol to HPL */
#define AIC31XX_LEFT_ANALOG_HPL		MAKE_REG(0, 1, 36)
/* Right Analog Vol to HPR */
#define AIC31XX_RIGHT_ANALOG_HPR	MAKE_REG(0, 1, 37)
/* Left Analog Vol to SPL */
#define AIC31XX_LEFT_ANALOG_SPL		MAKE_REG(0, 1, 38)
/* Right Analog Vol to SPR */
#define AIC31XX_RIGHT_ANALOG_SPR	MAKE_REG(0, 1, 39)
/* HPL Driver */
#define AIC31XX_HPL_DRIVER_REG		MAKE_REG(0, 1, 40)
/* HPR Driver */
#define AIC31XX_HPR_DRIVER_REG		MAKE_REG(0, 1, 41)
/* SPL Driver */
#define AIC31XX_SPL_DRIVER_REG		MAKE_REG(0, 1, 42)
/* SPR Driver */
#define AIC31XX_SPR_DRIVER_REG		MAKE_REG(0, 1, 43)
/* HP Driver Control */
#define AIC31XX_HP_DRIVER_CONTROL	MAKE_REG(0, 1, 44)
/*MICBIAS Configuration Register*/
#define AIC31XX_MICBIAS_CTRL_REG	MAKE_REG(0, 1, 46)
/* MIC PGA*/
#define AIC31XX_MICPGA_VOL_CTRL		MAKE_REG(0, 1, 47)
/* Delta-Sigma Mono ADC Channel Fine-Gain Input Selection for P-Terminal */
#define AIC31XX_MICPGA_PIN_CFG		MAKE_REG(0, 1, 48)
/* ADC Input Selection for M-Terminal */
#define AIC31XX_MICPGA_MIN_CFG		MAKE_REG(0, 1, 49)
/* Input CM Settings */
#define AIC31XX_MICPGA_CM_REG		MAKE_REG(0, 1, 50)

/* ****************** Page 3 Registers **************************************/

/* Timer Clock MCLK Divider */
#define AIC31XX_TIMER_MCLK_DIV		MAKE_REG(0, 3, 16)

/* ****************** Page 8 Registers **************************************/
#define AIC31XX_DAC_ADAPTIVE_BANK1_REG	MAKE_REG(0, 8, 1)


#define AIC31XX_ADC_DATAPATH_SETUP      MAKE_REG(0, 0, 81)
#define AIC31XX_DAC_DATAPATH_SETUP      MAKE_REG(0, 0, 63)

#endif
