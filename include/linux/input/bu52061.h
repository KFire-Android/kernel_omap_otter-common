#ifndef __BU52061_H__
#define __BU52061_H__
#include <linux/types.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <asm/atomic.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif

struct _bu52061_platform_data {
  struct input_dev *dev;     
  atomic_t used;
  struct work_struct irq_work;
  unsigned int irq;			
#if defined(CONFIG_HAS_WAKELOCK)
  struct wake_lock wake_lock;
#endif
  int (*init_irq)(void);
  int (*get_gpio_value)(void);

#ifdef CONFIG_HAS_EARLYSUSPEND
  struct early_suspend early_suspend;
#endif

};



#endif // __BU52061_H__

