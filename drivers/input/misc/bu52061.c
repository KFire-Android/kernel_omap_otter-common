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
#include <asm/uaccess.h>		
#include <linux/input.h>  

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/input/bu52061_ioctl.h>
//----------------------------------------------------
#define HALL_EFFECT 0  // Hall sensor output pin -- gpio_wk0

#define KEY_PRESSED 1
#define KEY_RELEASED 0

#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
#define TIMEOUT_VALUE 1200  // mini-second
struct timer_list hall_timer;
#endif

struct _bu52061_platform_data *g_bu52061_data = NULL;

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

static struct hall_event_info gHallEventInfo;

//----------------------------------------------------
#define BU52061_FTM_PORTING

#ifdef BU52061_FTM_PORTING
#include <linux/miscdevice.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu52061_early_suspend(struct early_suspend *es);
static void bu52061_late_resume(struct early_suspend *es);
#endif

#ifdef	CONFIG_PROC_FS      // Proc function
#define	BU52061_PROC_FILE	"driver/hall_sensor"
static struct proc_dir_entry *bu52061_proc_file;


static int bu52061_read_proc(char *page, char **start, off_t off, 
		int count, int*eof, void *data)
{
  u8 reg_val;
  reg_val = gHallEventInfo.hall_current_status;
  printk(KERN_DEBUG "bu52061_status=%d\n",gHallEventInfo.hall_current_status);
  return snprintf(page, count, "0x%x \n",reg_val);	
}

static int bu52061_write_proc(struct file *file, const char __user *buffer,
		unsigned long count, void *data)
{
  u8 msgbuf[count];	
  u8 val;
		
  if (copy_from_user(msgbuf, buffer, count))
    return -EFAULT;

  val=msgbuf[0];
  gHallEventInfo.hall_current_status = val;
  printk(KERN_DEBUG "bu52061_write_proc val=%d\n",val);	
  return count;
}

static void create_bu52061_proc_file(void)
{
  bu52061_proc_file = create_proc_entry(BU52061_PROC_FILE, 0644, NULL);
  if (bu52061_proc_file) {
    bu52061_proc_file->read_proc = bu52061_read_proc;
    bu52061_proc_file->write_proc = bu52061_write_proc;
  } else
  printk(KERN_ERR "bu52061 proc file create failed!\n");
}

static void remove_bu52061_proc_file(void)
{
  remove_proc_entry(BU52061_PROC_FILE, NULL);
}
#endif      //Proc function


static int bu52061_open(struct inode *inode, struct file *file)
{
  return 0;
}

static long bu52061_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  int ret = 0;
  struct ioctl_cmd data;

  memset(&data, 0, sizeof(data));

  switch(cmd) {
    case IOCTL_VALSET:
            if (!access_ok(VERIFY_READ,(void __user *)arg, _IOC_SIZE(cmd))) {
              ret = -EFAULT;
              goto done;
            }
            if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
              ret = -EFAULT;
              goto done;
            }

            printk(KERN_DEBUG "bu52061_ioctl received data = %d\n",data.halt_key);
            gHallEventInfo.ignore_hall_event = data.halt_key;
            break;
    default:
            ret = -EINVAL;
            break;
  }

  done:
    printk(KERN_DEBUG "bu52061_ioctl DONE \n");

  return ret;
}

static ssize_t bu52061_read(struct file *file, char __user * buffer, size_t size, loff_t * f_pos)
{	
  unsigned int val;
	
  val = gHallEventInfo.hall_current_status;
  // printk(KERN_INFO "Hall sensor state:%d\n", gHallEventInfo.hall_current_status);
	
  if(copy_to_user(buffer, &val, sizeof(val))){
    printk(KERN_ERR "[line:%d] copy_to_user failed\n",  __LINE__ );
    return -EFAULT;
  }
	
  return 0; 	
}

static ssize_t bu52061_write(struct file *file, const char __user *buffer, size_t size, loff_t *f_ops)
{
  return 0; 
}
static int bu52061_release(struct inode *inode, struct file *filp)
{
  return 0;
}

static void hall_handle_event(void)
{
  struct input_dev *dev = g_bu52061_data->dev;

  gHallEventInfo.hall_current_status = gpio_get_value(HALL_EFFECT);
  printk(KERN_INFO "BU52061: hall_current_status = %d\n",gHallEventInfo.hall_current_status);

  printk(KERN_DEBUG "BU52061: ignore_hall_event = %d\n",gHallEventInfo.ignore_hall_event);
  if (gHallEventInfo.ignore_hall_event == false) {
    /*Hall sensor State Machine: only two cases need to send power key event:
    1.close book-cover in Normal mode(BL_ON) 2.open book-cover in Suspend mode(BL_OFF) */
    if (((gHallEventInfo.bl_status == BL_ON) && (gHallEventInfo.hall_current_status == HALL_CLOSED)) ||
      ((gHallEventInfo.bl_status == BL_OFF) && (gHallEventInfo.hall_current_status == HALL_OPENED))) {
      input_report_key(dev, KEY_END, KEY_PRESSED);
      input_sync(dev);
      mdelay(20);
      input_report_key(dev, KEY_END, KEY_RELEASED);
      input_sync(dev);
    }
  }
}

#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
static void hall_timeout_report(unsigned long arg)
{
  hall_handle_event();
}

static void hall_init_timer(void)
{
  init_timer(&hall_timer);
  hall_timer.function = hall_timeout_report;
  hall_timer.data = 0;
  hall_timer.expires = jiffies + ((TIMEOUT_VALUE*HZ)/1000);
  add_timer(&hall_timer);
  printk(KERN_DEBUG "BU52061 hall_init_timer Done\n");
}
#endif

static void bu52061_irq_work(struct work_struct *work)
{
  hall_handle_event();
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
  mod_timer(&hall_timer, jiffies + ((TIMEOUT_VALUE*HZ)/1000));
  wake_lock_timeout(&g_bu52061_data->wake_lock, msecs_to_jiffies(TIMEOUT_VALUE));
#endif
}

static irqreturn_t bu52061_interrupt(int irq, void *dev_id)
{   	
  schedule_work(&g_bu52061_data->irq_work);
        
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
  .write = bu52061_write,
  .release = bu52061_release,	
  .unlocked_ioctl = bu52061_ioctl,
};

static struct miscdevice bu52061_dev =
{
  .minor = MISC_DYNAMIC_MINOR,
  .name = "bu52061",
  .fops = &bu52061_dev_fops,
};

static int __devinit bu52061_probe(struct platform_device *pdev)
{
  struct _bu52061_platform_data *bu52061_platform_data= pdev->dev.platform_data;
  struct input_dev *input_dev;
  int ret;

  g_bu52061_data= kzalloc(sizeof(struct _bu52061_platform_data), GFP_KERNEL);
  if (!g_bu52061_data) {
    ret = -ENOMEM;
    printk(KERN_ERR "[%s:%d] allocate g_bu52061_data fail!\n", __func__, __LINE__);
    goto fail1;
  }

#ifdef CONFIG_PROC_FS
  create_bu52061_proc_file();
#endif

	//input device  
  input_dev = input_allocate_device();
  if (!input_dev) {
    ret = -ENOMEM;
    printk(KERN_ERR "[%s:%d] allocate input device fail!\n", __func__, __LINE__);
    goto fail2;
  }
		
  bu52061_platform_data->dev = input_dev;
  //input device settings
  input_dev->name = "bu52061";
  input_dev->phys = "bu52061/input0";      
  input_dev->evbit[0] = BIT(EV_KEY);
  input_dev->keybit[BIT_WORD(KEY_END)] =
                        BIT_MASK(KEY_END);
  dev_set_drvdata(&input_dev->dev, bu52061_platform_data);
	
  input_dev->open = bu52061_input_open;
  input_dev->close = bu52061_input_close;

  ret = input_register_device(input_dev);
  if (ret) {
    printk(KERN_ERR "[%s:%d]bu52061 input register device fail!\n", __func__, __LINE__);
    goto fail2;
  }
	
  /* init irq */	
  g_bu52061_data=pdev->dev.platform_data;
  g_bu52061_data->dev=input_dev;
  g_bu52061_data->init_irq(); 
  enable_irq_wake(bu52061_platform_data->irq);

#ifdef CONFIG_HAS_EARLYSUSPEND
  g_bu52061_data->early_suspend.suspend = bu52061_early_suspend;
  g_bu52061_data->early_suspend.resume = bu52061_late_resume;
  register_early_suspend(&g_bu52061_data->early_suspend);
#endif

  gHallEventInfo.hall_current_status = gpio_get_value(HALL_EFFECT);
  gHallEventInfo.bl_status = BL_ON;
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM_FTM
  gHallEventInfo.ignore_hall_event = true;
#else
  gHallEventInfo.ignore_hall_event = false;
#endif

  INIT_WORK(&g_bu52061_data->irq_work, bu52061_irq_work);

  ret = request_irq(bu52061_platform_data->irq, bu52061_interrupt, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND, "bu52061_irq", NULL);
  if (ret) {
    printk(KERN_ERR"bu52061 request_irq failed, return:%d\n", ret);
    goto fail3;	
  }

#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
  hall_init_timer();
#if defined(CONFIG_HAS_WAKELOCK)
  wake_lock_init(&g_bu52061_data->wake_lock, WAKE_LOCK_SUSPEND, "Hall_Sensor");
#endif
#endif

  printk(KERN_INFO "BU52061 Probe OK\n");	
  return 0; 

fail3:
  input_free_device(input_dev);   	
fail2:
  kfree(g_bu52061_data);       
fail1:
  return ret;	
}

static int __devexit bu52061_remove(struct platform_device *pdev)
{
  struct _bu52061_platform_data *bu52061_platform_data= pdev->dev.platform_data;

  free_irq(bu52061_platform_data->irq, NULL);
  input_unregister_device(bu52061_platform_data->dev);
	
#ifdef CONFIG_PROC_FS
  remove_bu52061_proc_file();
#endif
  input_free_device(bu52061_platform_data->dev);
  gpio_free(HALL_EFFECT);
#ifdef CONFIG_HAS_EARLYSUSPEND
  unregister_early_suspend(&g_bu52061_data->early_suspend);
#endif

#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
  del_timer_sync(&hall_timer);
#if defined(CONFIG_HAS_WAKELOCK)
  wake_lock_destroy(&g_bu52061_data->wake_lock);
#endif
#endif
  kfree(g_bu52061_data);

  return 0;
}

#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
static void bu52061_shutdown(struct platform_device *pdev)
{
  struct _bu52061_platform_data *bu52061_platform_data= pdev->dev.platform_data;
  printk(KERN_INFO "bu52061: shutdown\n");
  free_irq(bu52061_platform_data->irq, NULL);
  cancel_work_sync(&g_bu52061_data->irq_work);
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu52061_early_suspend(struct early_suspend *es)
{
  gHallEventInfo.bl_status = BL_OFF;
}

static void bu52061_late_resume(struct early_suspend *es)
{
  gHallEventInfo.bl_status = BL_ON;
}
#endif


static int bu52061_suspend(struct device *dev)
{
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
#ifndef CONFIG_HAS_EARLYSUSPEND
  gHallEventInfo.bl_status = BL_OFF;
#endif
#else
  gHallEventInfo.bl_status = BL_OFF;
#endif
  gHallEventInfo.bl_status = BL_OFF;
  printk(KERN_INFO "bu52061_suspend\n");
  return 0;
}


static int bu52061_resume(struct device *dev)
{
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
#ifndef CONFIG_HAS_EARLYSUSPEND
  gHallEventInfo.bl_status = BL_ON;
#endif
#else
  if(gpio_get_value(HALL_EFFECT) == HALL_OPENED)
    gHallEventInfo.bl_status = BL_ON;
#endif

  printk(KERN_INFO "bu52061_resume\n");
  return 0;
}


static const struct dev_pm_ops bu52061_dev_pm_ops = {
  .suspend  = bu52061_suspend,
  .resume   = bu52061_resume,
};

static struct platform_driver bu52061_device_driver = {
  .probe    = bu52061_probe,
  .remove   = bu52061_remove,
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
  .shutdown = bu52061_shutdown,
#endif
  .driver   = {
    .name   = "bu52061",
    .owner  = THIS_MODULE,
    .pm     = &bu52061_dev_pm_ops,
    }
};

static int __init bu52061_init(void)
{
  printk(KERN_INFO "BU52061 sensors driver: init\n");

#ifdef BU52061_FTM_PORTING  
  if (0 != misc_register(&bu52061_dev))
  {
    printk(KERN_ERR "bu52061_dev register failed.\n");
    return 0;
  }
else
  {
    printk(KERN_INFO "bu52061_dev register ok.\n");
  }
#endif                     

  return platform_driver_register(&bu52061_device_driver);
}

static void __exit bu52061_exit(void)
{
  printk(KERN_INFO "BU52061 sensors driver: exit\n");

#ifdef BU52061_FTM_PORTING
  misc_deregister(&bu52061_dev);
#endif
  platform_driver_unregister(&bu52061_device_driver);
}

module_init(bu52061_init);
module_exit(bu52061_exit);

MODULE_DESCRIPTION("Rohm BU52061NVX hall sensor driver");
MODULE_AUTHOR("Joss");
MODULE_LICENSE("GPL");
