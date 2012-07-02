/*
 *
 * CEC keyboard driver
 *
 * Author: Muralidhar Dixit <murali.dixit@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _CEC_KEYBOARD_H_
#define _CEC_KEYBOARD_H_

/**********************************************************
* defines the function pointers to be used by CEC driver
* to inform the CEC events to cec_keyboard driver
**********************************************************/
struct cec_keyboard_device {
	int (*connect)(unsigned char status);
	int (*key_event)(unsigned char val, unsigned char pressed);
};

/*Function to get the handles for CEC keyboard driver*/
int cec_keyboard_get_device(struct cec_keyboard_device *dev);

#endif
