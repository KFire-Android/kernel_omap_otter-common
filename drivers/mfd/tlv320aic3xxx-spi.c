#include <linux/err.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <linux/mfd/tlv320aic3xxx-core.h>
#include <linux/mfd/tlv320aic31xx-registers.h>
#include <linux/mfd/tlv320aic3xxx-registers.h>

#if 0
int aic3xxx_spi_read_device(struct aic3xxx *aic3xxx, u8 offset, void *dest, int count)
{
        struct spi_device *spi = to_spi_device(aic3xxx->dev);
        int ret;

	ret = spi_write_then_read(spi, offset, 1, dest, count);
        if (ret < 0)
                return ret;
        
        return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_spi_read_device);
#endif

int aic3xxx_spi_write_device(struct aic3xxx *aic3xxx ,u8 offset,
                                                const void *src,int count)
{
        struct spi_device *spi = to_spi_device(aic3xxx->dev);
        u8 write_buf[count+1];
	int ret;

	write_buf[0] = offset;
	memcpy(&write_buf[1], src, count);
	ret = spi_write(spi, write_buf, count + 1);

        if (ret < 0)
                return ret;

        return 0;
}
EXPORT_SYMBOL_GPL(aic3xxx_spi_write_device);

static __devinit int aic3xxx_spi_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct aic3xxx *aic3xxx;
	aic3xxx = kzalloc(sizeof(*aic3xxx), GFP_KERNEL);
	if (aic3xxx == NULL)
		return -ENOMEM;
	spi_set_drvdata(spi, aic3xxx);
	aic3xxx->dev = &spi->dev;
	aic3xxx->type = id->driver_data;
	aic3xxx->irq = spi->irq;

	return aic3xxx_device_init(aic3xxx);
}

static int __devexit aic3xxx_spi_remove(struct spi_device *spi)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(&spi->dev);
	aic3xxx_device_exit(aic3xxx);
	return 0;
}

static const struct spi_device_id aic3xxx_spi_id[] = {
	{"tlv320aic31xx", TLV320AIC31XX},
	{ }
};
MODULE_DEVICE_TABLE(spi, aic3xxx_spi_id);

static struct spi_driver aic3xxx_spi_driver = {
	.driver = {
		.name	= "tlv320aic3xxx",
		.owner	= THIS_MODULE,
	},
	.probe		= aic3xxx_spi_probe,
	.remove		= __devexit_p(aic3xxx_spi_remove),
	.id_table	= aic3xxx_spi_id,
};

module_spi_driver(aic3xxx_spi_driver);

MODULE_DESCRIPTION("TLV320AIC3XXX SPI bus interface");
MODULE_AUTHOR("Mukund Navada <navada@ti.com>");
MODULE_LICENSE("GPL");
