/*
 * Copyright (C) ST-Ericsson AB 2012
 * Author:	Sjur Brendeland / sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CAIF_SERIAL_H_
#define CAIF_SERIAL_H_
void power_notify(bool is_on);
void cwr_notify(struct tty_struct *tty, bool asserted);
#ifdef CONFIG_CAIF_TTY_PM
extern int pm_ext_init(struct tty_struct *tty);
extern void pm_ext_deinit(struct tty_struct *tty);
extern void pm_ext_prep_down(struct tty_struct *tty);
extern void pm_ext_down(struct tty_struct *tty);
extern void pm_ext_up(struct tty_struct *tty);
#endif
#endif /* CAIF_SERIAL_H_ */
