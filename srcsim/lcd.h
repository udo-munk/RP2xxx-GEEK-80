/*
 * Functions for using the RP2040/RP2350-GEEK LCD from the emulation
 *
 * Copyright (C) 2024 by Udo Munk & Thomas Eberhardt
 */

#ifndef LCD_INC
#define LCD_INC

#include "sim.h"
#include "simdefs.h"

#include "lcd_dev.h"
#include "draw.h"

#define LCD_STATUS_CURRENT	0
#define LCD_STATUS_REGISTERS	1
#define LCD_STATUS_PANEL	2
#define LCD_STATUS_DRIVES	3
#define LCD_STATUS_MEMORY	4

typedef void (*lcd_func_t)(bool first);

extern uint16_t led_color;	/* call lcd_update_led() after changing this */

extern void lcd_init(void), lcd_exit(void);
extern void lcd_brightness(int brightness);
extern void lcd_set_rotation(bool rotated);
extern void lcd_update_led(void);
extern void lcd_custom_disp(lcd_func_t draw_func);
extern void lcd_status_disp(int which);
extern void lcd_status_next(void);
extern void lcd_update_drive(int drive, int track, int sector, WORD addr,
			     bool rdwr, bool active);

#endif /* !LCD_INC */
