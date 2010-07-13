/* hwspinlock.h */

#ifndef HWSPINLOCK_H
#define HWSPINLOCK_H

#include <linux/platform_device.h>
#include <plat/omap44xx.h>

/* Read values from the spinlock register */
#define HWSPINLOCK_ACQUIRED 0
#define HWSPINLOCK_BUSY 1

/* Device data */
struct hwspinlock_plat_info {
	u32 sysstatus_offset;		/* System status register offset */
	u32 lock_base_offset;		/* Offset of spinlock registers */
};

struct hwspinlock;

int hwspinlock_lock(struct hwspinlock *handle);
int hwspinlock_trylock(struct hwspinlock *handle);
int hwspinlock_unlock(struct hwspinlock *handle);

struct hwspinlock *hwspinlock_request(void);
struct hwspinlock *hwspinlock_request_specific(unsigned int id);
int hwspinlock_free(struct hwspinlock *hwspinlock_ptr);

#endif /* HWSPINLOCK_H */
