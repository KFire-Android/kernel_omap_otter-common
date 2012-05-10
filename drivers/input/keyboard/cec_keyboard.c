/*
* linux/drivers/input/keyboard/cec_keyboard.c
*
* CEC Keyboard driver
*
* Copyright (C) 2012 Texas Instruments, Inc
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

#include <linux/kernel.h>
#include <linux/input/cec_keyboard.h>
#include <linux/input.h>
#include <video/cec.h>

static const char name[] = "cec_keyboard";

struct cec_keyboard {
	struct input_dev *cec_input_dev;
	struct cec_dev cecdev;
} cec_keyboard_dev;

static const int cec_keymap[] = {
	KEY_ENTER,/*0*/
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	-1,/*5*/
	-1,
	-1,
	-1,
	KEY_HOME,
	-1,/*0xa--setup menu*/
	-1,/*0xb-content menu*/
	-1,/*0xc fav menu*/
	KEY_BACK,/*0xd*/
	-1,
	-1,
	-1,/*0x10*/
	-1,
	-1,
	-1,/*0x13*/
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,/*0x1c*/
	-1,/*0x1d -Num entry mode*/
	-1,/*0x1e-num 11*/
	-1,/*0x1f-num 12*/
	KEY_0,/*0x20*/
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,/*0x29*/
	KEY_DOT,
	KEY_ENTER,
	KEY_CLEAR,
	-1,
	-1,/**/
	-1,/*0x2f-next fav*/
	KEY_CHANNELUP,
	KEY_CHANNELDOWN,
	-1,
	-1,
	-1,
	-1,/*0x35-display info*/
	KEY_HELP,
	KEY_PAGEUP,
	KEY_PAGEDOWN,
	-1,/*0x39*/
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	KEY_POWER,/*0x40*/
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_MUTE,
	KEY_PLAY,
	KEY_STOP,/*0x45*/
	KEY_PAUSE,
	KEY_RECORD,
	KEY_REWIND,
	KEY_FASTFORWARD,
	KEY_EJECTCD,/*0x4a*/
	KEY_FORWARD,
	KEY_BACK,
	KEY_STOP,/*0x4d-stop record*/
	KEY_PAUSE,/*Pause record*/
	KEY_ANGLE,/*0x50*/
	-1,/*SUB Picture*/
	-1,/*Video on demand*/
	KEY_EPG,
	-1,/*0x54-Time Programming*/
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,/*0x5a*/
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,/*0x60*/
	-1,
	-1,
	-1,
	-1,
	-1,/*0x65*/
	-1,
	-1,
	-1,
	-1,
	-1,/*0x6a*/
	-1,
	KEY_POWER,/*0x6c-Power OFF*/
	KEY_POWER,/*0x6d-power ON*/
	-1,
	-1,
	-1,/*0x70*/
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	-1
};

static int cec_keyboard_register(void)
{
	int r;
	int i;
	pr_debug("CEC_KEYBOARD: allocate input device\n");
	cec_keyboard_dev.cec_input_dev = input_allocate_device();
	if (!cec_keyboard_dev.cec_input_dev) {
		pr_err("CEC:KEYBOARD: failed to allcate input device\n");
		r = -ENOMEM;
		return r;

	}

	cec_keyboard_dev.cec_input_dev->name = name;
	__set_bit(EV_KEY, cec_keyboard_dev.cec_input_dev->evbit);

	for (i = 0; i < (sizeof(cec_keymap)/sizeof(int)); i++) {
		if (cec_keymap[i] != -1)
			__set_bit(cec_keymap[i],
				cec_keyboard_dev.cec_input_dev->keybit);
	}
	pr_debug("CEC_KEYBOARD: set input capability\n");
	input_set_capability(cec_keyboard_dev.cec_input_dev, EV_KEY, MSC_RAW);
	pr_debug("CEC_KEYBOARD: register input device\n");

	r = input_register_device(cec_keyboard_dev.cec_input_dev);
	if (r < 0) {
		pr_err("CEC_KEYBOARD:failed to register input device\n");
		input_free_device(cec_keyboard_dev.cec_input_dev);
		return r;
	}
	return 0;

}

static int cec_keyboard_unregister(void)
{
	input_unregister_device(cec_keyboard_dev.cec_input_dev);
	input_free_device(cec_keyboard_dev.cec_input_dev);
	return 0;
}

static int on_hdmi_connect(unsigned char status)
{
	int r = 0;

	if (status) {
		/*HDMI is connected*/
		pr_debug("CEC_KEYBOARD:HDMI connected\n");


		r = cec_keyboard_register();
		if (r)
			return r;

		/*Enable UI call backs from the CEC driver*/
		r = cec_enable_ui_event(true);
	} else
		cec_keyboard_unregister();

	return r;
}

static int cec_ui_event(unsigned char val, unsigned char pressed)
{

	if (val > (sizeof(cec_keymap)/sizeof(int)))
		return 0;

	else if (cec_keymap[val] >= 0) {
		/*TODO: For some keys we might have to send the
			key value as zero
		*/
		input_report_key(cec_keyboard_dev.cec_input_dev,
			cec_keymap[val], pressed);
		input_sync(cec_keyboard_dev.cec_input_dev);
	}

	return 0;
}

int cec_keyboard_get_device(struct cec_keyboard_device *dev)
{
	if (dev == NULL)
		return -EINVAL;

	dev->connect = on_hdmi_connect;
	dev->key_event = cec_ui_event;

	return 0;
}
EXPORT_SYMBOL(cec_keyboard_get_device);

static int __init cec_keyboard_init(void)
{
	return 0;

}

static void __exit cec_keyboard_exit(void)
{
	cec_keyboard_dev.cec_input_dev = NULL;

}

/*-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */
early_initcall(cec_keyboard_init);
module_exit(cec_keyboard_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CEC Keboard  module");
MODULE_AUTHOR("Muralidhar Dixit");
