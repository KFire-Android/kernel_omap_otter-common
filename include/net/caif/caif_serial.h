/*
 * Copyright (C) ST-Ericsson AB 2012
 * Author:	Sjur Brendeland / sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CAIF_SERIAL_H_
#define CAIF_SERIAL_H_
void power_notify(bool is_on);
void cwr_notify(struct tty_struct *tty, bool asserted);
#endif /* CAIF_SERIAL_H_ */
