#ifndef __VIDEO_GENERIC_BOARD_DATA_H__
#define __VIDEO_GENERIC_BOARD_DATA_H__

struct panel_board_data {
	int x_res;
	int y_res;
	int reset_gpio;
	int lcd_en_gpio;
	int lcd_vsys_gpio;
	int cabc_en_gpio;
};

#endif
