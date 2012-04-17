/*
 * header for twl and similar RTC driver
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

struct twl_rtc_data {
	void *mfd;
	int chip_version;
	int (*read_block)(void *mfd, u8 *dest, u8 reg, int no_regs);
	int (*write_block)(void *mfd, u8 *data, u8 reg, int no_regs);

	int (*mask_irq)(void *mfd);
	int (*unmask_irq)(void *mfd);
	int (*clear_irq)(void *mfd);
};
