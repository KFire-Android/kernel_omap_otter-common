
#ifndef __MFD_AIC31XX_CORE_H__
#define __MFD_AIC31XX_CORE_H__

#include <linux/interrupt.h>
#include <linux/mfd/core.h>
enum aic3xxx_type {
	TLV320AIC31XX = 0,
};


#define AIC31XX_IRQ_HEADSET_DETECT	0
#define AIC31XX_IRQ_BUTTON_PRESS	1
#define AIC31XX_IRQ_DAC_DRC		2
#define AIC31XX_IRQ_AGC_NOISE		3
#define AIC31XX_IRQ_OVER_CURRENT	4
#define AIC31XX_IRQ_OVERFLOW_EVENT	5
#define AIC31XX_IRQ_SPEAKER_OVER_TEMP	6

#define AIC31XX_GPIO1			7

typedef union aic3xxx_reg_union {
	struct aic3xxx_reg {
	u8 offset;
	u8 page;
	u8 book;
	u8 reserved;
	} aic3xxx_register;
	unsigned int aic3xxx_register_int;
} aic31xx_reg_union;


/****************************************************************************/

/*
 *****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic3xxx_setup_data |
 *          i2c specific data setup for AIC31XX.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic3xxx_setup_data {
	unsigned short i2c_address;
};

/* GPIO API */
#define AIC31XX_NUM_GPIO 1 /*only one GPIO present*/
enum {
	AIC31XX_GPIO1_FUNC_DISABLED		= 0,
	AIC31XX_GPIO1_FUNC_INPUT		= 1,
	AIC31XX_GPIO1_FUNC_OUTPUT		= 3,
	AIC31XX_GPIO1_FUNC_CLOCK_OUTPUT		= 4,
	AIC31XX_GPIO1_FUNC_INT1_OUTPUT		= 5,
	AIC31XX_GPIO1_FUNC_INT2_OUTPUT		= 6,
	AIC31XX_GPIO1_FUNC_ADC_MOD_CLK_OUTPUT	= 10,

};

struct aic3xxx_configs {
	u8 book_no;
	u16 reg_offset;
	u8 reg_val;
};
#if 0
struct aic3xxx_gpio_setup {
       u8 used; /*GPIO, GPI and GPO is used in the board, used = 1 else 0*/
       u8 in; /*GPIO is used as input, in = 1 else
                                in = 0. GPI in = 1, GPO in = 0*/
       unsigned int in_reg; /* if GPIO is input,
                               register to write the mask to.*/
       u8 in_reg_bitmask; /* bitmask for 'value' to be written into in_reg*/
       u8 in_reg_shift; /* bits to shift to write 'value' into in_reg*/
       u8 value; /* value to be written gpio_control_reg if GPIO is output,
                               in_reg if its input*/
       unsigned int reg;
};
#endif

struct aic3xxx_gpio_setup {
	unsigned int reg;
	u8 value;
};

struct aic3xxx_pdata {
	unsigned int audio_mclk1;
	unsigned int audio_mclk2;
	unsigned int gpio_irq; /* whether AIC31XX interrupts the host AP
				on a GPIO pin of AP */
	unsigned int gpio_reset; /* is the codec being reset by a gpio
				[host] pin, if yes provide the number. */
	int num_gpios;

	struct aic3xxx_gpio_setup *gpio_defaults;/* all gpio configuration */
	int naudint_irq; /* audio interrupt */
	unsigned int irq_base;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic3xxx_rate_divs |
 *          Setting up the values to get different freqencies
 *
 * @field   u32 | mclk |
 *          Master clock
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | aosr |
 *          value for aosr
 * @field   u32 | nadc |
 *          value for nadc
 * @field   u32 | madc |
 *          value for madc
 * @field   u32 | blck_N |
 *          value for block N
 */
struct aic3xxx {
	struct mutex io_lock;
	struct mutex irq_lock;
	enum aic3xxx_type type;
	struct device *dev;
	struct aic3xxx_pdata pdata;
	unsigned int irq;
	unsigned int irq_base;

	u8 irq_masks_cur;
	u8 irq_masks_cache;

	/* Used over suspend/resume */
	bool suspended;

	u8 book_no;
	u8 page_no;
};

static inline int aic3xxx_request_irq(struct aic3xxx *aic3xxx, int irq,
				      irq_handler_t handler,
				      unsigned long irqflags, const char *name,
				      void *data)
{
	if (!aic3xxx->irq_base)
		return -EINVAL;

	return request_threaded_irq(irq, NULL, handler,
				    irqflags, name, data);
}

static inline int aic3xxx_free_irq(struct aic3xxx *aic3xxx, int irq, void *data)
{
	if (!aic3xxx->irq_base)
		return -EINVAL;

	free_irq(aic3xxx->irq_base + irq, data);
	return 0;
}

/* Device I/O API */
int aic3xxx_reg_read(struct aic3xxx *aic3xxx, unsigned int reg);
int aic3xxx_reg_write(struct aic3xxx *aic3xxx, unsigned int reg,
		unsigned char val);
int aic3xxx_set_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		unsigned char mask, unsigned char val);
int aic3xxx_bulk_read(struct aic3xxx *aic3xxx, unsigned int reg,
		int count, u8 *buf);
int aic3xxx_bulk_write(struct aic3xxx *aic3xxx, unsigned int reg,
		       int count, const u8 *buf);
int aic3xxx_wait_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		      unsigned char mask, unsigned char val, int delay,
		      int counter);

int aic3xxx_irq_init(struct aic3xxx *aic3xxx);
void aic3xxx_irq_exit(struct aic3xxx *aic3xxx);
int aic3xxx_device_init(struct aic3xxx *aic3xxx);
void aic3xxx_device_exit(struct aic3xxx *aic3xxx);
int aic3xxx_i2c_read_device(struct aic3xxx *aic3xxx, u8 offset,
				void *dest, int count);
int aic3xxx_i2c_write_device(struct aic3xxx *aic3xxx, u8 offset,
				const void *src, int count);

//int aic3xxx_spi_read_device(struct aic3xxx *aic3xxx, u8 offset,
//				void *dest, int count);
int aic3xxx_spi_write_device(struct aic3xxx *aic3xxx, u8 offset,
				const void *src, int count);

#endif /* End of __MFD_AIC31XX_CORE_H__ */
