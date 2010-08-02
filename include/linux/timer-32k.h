/*
 * OMAP2/3/4 timer32k driver interface
 *
 * Copyright (C) 2010 Texas Instruments, Inc
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef __LINUX_TIMER_32_H__
#define __LINUX_TIMER_32_H__

#include <linux/ioctl.h>
#define OMAP_32K_READ       _IOWR('t', 0, uint32_t)
#define OMAP_32K_READRAW    _IOWR('t', 1, uint32_t)

#ifndef __KERNEL__

#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static inline uint32_t __omap_32k_call(int nr)
{
	static int fd;
	if (!fd)
		fd = open("/dev/timer32k", 0);

	if (fd) {
		uint32_t t;
		if (ioctl(fd, nr, &t) >= 0)
			return t;
	}
	return 0;
}
static inline uint32_t omap_32k_read(void)
{
	return __omap_32k_call(OMAP_32K_READ);
}
static inline uint32_t omap_32k_readraw(void)
{
	return __omap_32k_call(OMAP_32K_READRAW);
}
#endif

#endif /* __LINUX_TIMER_32_H__ */

