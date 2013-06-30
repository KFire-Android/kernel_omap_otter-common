#include <linux/module.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM
#define JEM_DEV_MODEL_NAME "JEM"
#endif

static struct proc_dir_entry *proc_entry;

static int devinfo_read( char *page, char **start, off_t off,
                   int count, int *eof, void *data )
{
#ifdef JEM_DEV_MODEL_NAME
	sprintf(page, "%s\n", JEM_DEV_MODEL_NAME);
#else
	sprintf(page, "%s\n", "unknown");
#endif

	return strlen(page);
}

static int __devinit jem_devmodel_probe(struct platform_device *pdev)
{
	int ret = 0;

	proc_entry = create_proc_entry("devmodel", 0444, NULL);

	if (proc_entry == NULL)
	{
		ret = -ENOMEM;
		pr_err("devmodel: Couldn't create proc entry\n");
	}
	else
	{
		proc_entry->read_proc = devinfo_read;
		proc_entry->write_proc = 0;
	}
	return 0;
}

static int __devexit jem_devmodel_remove(struct platform_device *pdev)
{
	remove_proc_entry("devmodel", NULL);
	return 0;
}

static struct platform_driver jem_devmodel_driver = {
	.probe		= jem_devmodel_probe,
	.remove		= __devexit_p(jem_devmodel_remove),
	.driver		= {
		.name	= "jem_devmodel",
		.owner	= THIS_MODULE,
	},
};

static int __init jem_devmodel_init(void)
{
	return platform_driver_register(&jem_devmodel_driver);
}
module_init(jem_devmodel_init);

static void __exit jem_devmodel_exit(void)
{
	platform_driver_unregister(&jem_devmodel_driver);
}
module_exit(jem_devmodel_exit);

