/*
 * Rohm BU52061NVX hall sensor driver
 * driver/input/misc/bu52061.c
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input/bu52061.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/input.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/input/bu52061_ioctl.h>

#include <linux/miscdevice.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif

#define DEVICE_NAME		"bu52061"
#define KEY_PRESSED 1
#define KEY_RELEASED 0

#define TIMEOUT_VALUE 1200 /* milliseconds */
struct timer_list hall_timer;

enum backlight_status {
	BL_OFF = 0,
	BL_ON
};

enum hall_status {
	HALL_CLOSED = 0,
	HALL_OPENED
};

struct hall_event_info {
	enum backlight_status bl_status;
	enum hall_status hall_current_status;
	unsigned int ignore_hall_event;
};

struct bu52061_context {
	struct platform_device *pdev;
	struct input_dev *dev;
	struct hall_event_info event;
	struct work_struct irq_work;
	atomic_t used;
	unsigned int gpio;
#if defined(CONFIG_HAS_WAKELOCK)
	struct wake_lock wake_lock;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu52061_early_suspend(struct early_suspend *es);
static void bu52061_late_resume(struct early_suspend *es);
#endif

#ifdef	CONFIG_PROC_FS
#define	BU52061_PROC_FILE	"driver/hall_sensor"
static int bu52061_read_proc(char *page,
	char **start, off_t off, int count, int *eof, void *data)
{
	struct bu52061_context *ctx = data;
	u8 reg_val = ctx->event.hall_current_status;

	printk(KERN_DEBUG "bu52061_status=%d\n", reg_val);

	return snprintf(page, count, "0x%x\n", reg_val);
}

static int bu52061_write_proc(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	struct bu52061_context *ctx = data;
	u8 msgbuf[count];
	u8 val;

	if (copy_from_user(msgbuf, buffer, count))
		return -EFAULT;

	val = msgbuf[0];
	ctx->event.hall_current_status = val;
	printk(KERN_DEBUG "bu52061_write_proc val=%d\n", val);

	return count;
}

static void create_bu52061_proc_file(struct bu52061_context *ctx)
{
	struct proc_dir_entry *proc_entry;

	proc_entry = create_proc_entry(BU52061_PROC_FILE, 0644, NULL);
	if (!proc_entry) {
		pr_err("bu52061 proc file create failed!\n");
		return;
	}

	proc_entry->read_proc = bu52061_read_proc;
	proc_entry->write_proc = bu52061_write_proc;
	proc_entry->data = ctx;
}

static void remove_bu52061_proc_file(void)
{
	remove_proc_entry(BU52061_PROC_FILE, NULL);
}
#endif	/* CONFIG_PROC_FS */


static int bu52061_open(struct inode *inode, struct file *file)
{
	struct device *dev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, DEVICE_NAME);
	if (!dev || !dev->driver)
		return -EBADF;

	file->private_data = dev_get_drvdata(dev);

	return 0;
}

static long bu52061_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct bu52061_context *ctx = f->private_data;
	struct device *dev = &ctx->pdev->dev;
	struct ioctl_cmd data;
	int ret = 0;

	memset(&data, 0, sizeof(data));

	if (cmd == IOCTL_VALSET
		&& access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))
		&& !copy_from_user(&data, (int __user *)arg, sizeof(data))) {

		dev_dbg(dev, "ioctl received data = %d\n", data.halt_key);
		ctx->event.ignore_hall_event = data.halt_key;
	} else
		ret = -EINVAL;

	dev_dbg(dev, "ioctl DONE\n");

	return ret;
}

static ssize_t bu52061_read(struct file *filp,
	char __user *buffer, size_t size, loff_t *f_pos)
{
	struct bu52061_context *ctx = filp->private_data;
	struct device *dev = &ctx->pdev->dev;
	unsigned int val;

	val = ctx->event.hall_current_status;

	if (copy_to_user(buffer, &val, sizeof(val))) {
		dev_err(dev, "[line:%d] copy_to_user failed\n", __LINE__);
		return -EFAULT;
	}

	return size;
}

static void hall_handle_event(struct bu52061_context *ctx)
{
	struct input_dev *dev = ctx->dev;

	ctx->event.hall_current_status = gpio_get_value(ctx->gpio);

	dev_dbg(&dev->dev, "status = %d, event = %d\n",
		ctx->event.hall_current_status, ctx->event.ignore_hall_event);

	if (ctx->event.ignore_hall_event)
		return;

	/* Hall sensor State Machine:
	 * only two cases need to send power key event:
	 * 1.close book-cover in Normal mode(BL_ON)
	 * 2.open book-cover in Suspend mode(BL_OFF)
	 */
	if (((ctx->event.bl_status == BL_ON)
		&& (ctx->event.hall_current_status == HALL_CLOSED))
	    || ((ctx->event.bl_status == BL_OFF)
		&& (ctx->event.hall_current_status == HALL_OPENED))) {
		input_report_key(dev, KEY_END, KEY_PRESSED);
		input_sync(dev);
		mdelay(20);
		pr_info("Hall sensor event : lid %s\n",
			ctx->event.hall_current_status ? "opened" : "closed");
		input_report_key(dev, KEY_END, KEY_RELEASED);
		input_sync(dev);
	}
}

static void hall_timeout_report(unsigned long arg)
{
	struct bu52061_context *ctx = (struct bu52061_context *)arg;

	hall_handle_event(ctx);
}

static void hall_init_timer(struct bu52061_context *ctx)
{
	struct device *dev = &ctx->pdev->dev;

	init_timer(&hall_timer);
	hall_timer.function = hall_timeout_report;
	hall_timer.data = (unsigned long)ctx;
	hall_timer.expires = jiffies + ((TIMEOUT_VALUE*HZ)/1000);
	add_timer(&hall_timer);
	dev_dbg(dev, "hall_init_timer Done\n");
}

static void bu52061_irq_work(struct work_struct *work)
{
	struct bu52061_context *ctx;

	ctx = container_of(work, struct bu52061_context, irq_work);

	hall_handle_event(ctx);
	mod_timer(&hall_timer, jiffies + ((TIMEOUT_VALUE*HZ)/1000));
	wake_lock_timeout(&ctx->wake_lock, msecs_to_jiffies(TIMEOUT_VALUE));
}

static irqreturn_t bu52061_interrupt(int irq, void *dev_id)
{
	struct bu52061_context *ctx = dev_id;

	schedule_work(&ctx->irq_work);

	return IRQ_HANDLED;
}

static int bu52061_input_open(struct input_dev *dev)
{
	return 0;
}

static void bu52061_input_close(struct input_dev *dev)
{
}

static const struct file_operations bu52061_dev_fops = {
	.owner = THIS_MODULE,
	.open = bu52061_open,
	.read = bu52061_read,
	.unlocked_ioctl = bu52061_ioctl,
};

static struct miscdevice bu52061_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "bu52061",
	.fops = &bu52061_dev_fops,
};

static int __devinit bu52061_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bu52061_platform_data *pdata = dev_get_platdata(dev);
	struct bu52061_context *ctx;
	struct input_dev *input_dev;
	int ret, mask, bu52061_irq;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

#ifdef CONFIG_PROC_FS
	create_bu52061_proc_file(ctx);
#endif

	/* Input device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		kfree(ctx);
		dev_err(dev, "[%s:%d] allocate input device fail!\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto fail1;
	}

	ctx->pdev = pdev;
	ctx->dev = input_dev;
	ctx->gpio = pdata->gpio;

	dev_set_drvdata(&input_dev->dev, ctx);
	dev_set_drvdata(dev, ctx);

	input_dev->name = "bu52061";
	input_dev->phys = "bu52061/input0";
	input_dev->evbit[0] = BIT(EV_KEY);
	input_dev->keybit[BIT_WORD(KEY_END)] = BIT_MASK(KEY_END);
	input_dev->open = bu52061_input_open;
	input_dev->close = bu52061_input_close;
	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(dev, "[%s:%d]bu52061 input register device fail!\n",
			__func__, __LINE__);
		goto fail2;
	}

	/* Init irq */
	ret = gpio_request_one(ctx->gpio, GPIOF_DIR_IN, "hall_sensor_status");
	if (ret) {
		dev_err(dev, "[%s:%d]bu52061 cannot claim sensor GPIO!\n",
			__func__, __LINE__);
		goto fail2;
	}

	bu52061_irq =  gpio_to_irq(ctx->gpio);
	mask = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND;
	ret = request_irq(bu52061_irq, bu52061_interrupt, mask, "bu52061", ctx);
	if (ret) {
		dev_err(dev, "bu52061 request_irq failed, return:%d\n", ret);
		goto fail2;
	}

	enable_irq_wake(bu52061_irq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ctx->early_suspend.suspend = bu52061_early_suspend;
	ctx->early_suspend.resume = bu52061_late_resume;
	register_early_suspend(&ctx->early_suspend);
#endif

	ctx->event.hall_current_status = gpio_get_value(ctx->gpio);
	ctx->event.bl_status = BL_ON;
	ctx->event.ignore_hall_event = false;

	INIT_WORK(&ctx->irq_work, bu52061_irq_work);

	hall_init_timer(ctx);
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_init(&ctx->wake_lock, WAKE_LOCK_SUSPEND, "Hall_Sensor");
#endif

	printk(KERN_INFO "BU52061 Probe OK\n");
	return 0;

fail2:
	input_free_device(input_dev);
fail1:
	return ret;
}

static int __devexit bu52061_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bu52061_context *ctx = dev_get_drvdata(dev);

	dev_info(dev, "Remove\n");

	free_irq(gpio_to_irq(ctx->gpio), ctx);
	del_timer_sync(&hall_timer);
	cancel_work_sync(&ctx->irq_work);
	input_unregister_device(ctx->dev);
	input_free_device(ctx->dev);
	gpio_free(ctx->gpio);

#ifdef CONFIG_PROC_FS
	remove_bu52061_proc_file();
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ctx->early_suspend);
#endif

#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_destroy(&ctx->wake_lock);
#endif

	dev_set_drvdata(dev, NULL);
	kfree(ctx);

	return 0;
}

static void bu52061_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bu52061_context *ctx = dev_get_drvdata(dev);

	dev_info(dev, "Shutdown\n");
	free_irq(gpio_to_irq(ctx->gpio), ctx);
	del_timer_sync(&hall_timer);
	cancel_work_sync(&ctx->irq_work);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu52061_early_suspend(struct early_suspend *es)
{
	struct bu52061_context *ctx;

	ctx = container_of(es, struct bu52061_context, early_suspend);

	ctx->event.bl_status = BL_OFF;
}

static void bu52061_late_resume(struct early_suspend *es)
{
	struct bu52061_context *ctx;

	ctx = container_of(es, struct bu52061_context, early_suspend);

	ctx->event.bl_status = BL_ON;
}
#endif


static int bu52061_suspend(struct device *dev)
{
	struct bu52061_context *ctx = dev_get_drvdata(dev);

	ctx->event.bl_status = BL_OFF;
	dev_info(dev, "Suspend\n");
	return 0;
}


static int bu52061_resume(struct device *dev)
{
	struct bu52061_context *ctx = dev_get_drvdata(dev);

	if (gpio_get_value(ctx->gpio) == HALL_OPENED)
		ctx->event.bl_status = BL_ON;

	dev_info(dev, "Resume\n");
	return 0;
}


static const struct dev_pm_ops bu52061_dev_pm_ops = {
	.suspend  = bu52061_suspend,
	.resume   = bu52061_resume,
};

static struct platform_driver bu52061_device_driver = {
	.probe    = bu52061_probe,
	.remove   = bu52061_remove,
	.shutdown = bu52061_shutdown,
	.driver   = {
		.name   = DEVICE_NAME,
		.owner  = THIS_MODULE,
		.pm     = &bu52061_dev_pm_ops,
	}
};

static int __init bu52061_init(void)
{
	printk(KERN_INFO "BU52061 sensors driver: init\n");

	if (misc_register(&bu52061_dev)) {
		pr_err("bu52061_dev register failed.\n");
		return 0;
	}

	pr_info("bu52061_dev register ok.\n");

	return platform_driver_register(&bu52061_device_driver);
}

static void __exit bu52061_exit(void)
{
	printk(KERN_INFO "BU52061 sensors driver: exit\n");

	misc_deregister(&bu52061_dev);
	platform_driver_unregister(&bu52061_device_driver);
}

module_init(bu52061_init);
module_exit(bu52061_exit);

MODULE_DESCRIPTION("Rohm BU52061NVX hall sensor driver");
MODULE_AUTHOR("Joss");
MODULE_LICENSE("GPL");
