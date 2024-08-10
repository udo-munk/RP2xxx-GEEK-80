/*
 * Emulation of Cromemco Dazzler on the RP2040-GEEK LCD
 *
 * Copyright (C) 2015-2019 by Udo Munk
 * Copyright (C) 2024 by Thomas Eberhardt
 */

#include <stdint.h>
#include "pico.h"
#include "pico/time.h"

#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simcfg.h"
#include "simmem.h"
#include "simport.h"

#include "dazzler.h"
#include "lcd.h"

/* Graphics stuff */
#if LCD_COLOR_DEPTH == 12
/* 444 colors and grays */
static const uint16_t __not_in_flash("color_map") colors[16] = {
	0x0000, 0x0800, 0x0080, 0x0880,
	0x0008, 0x0808, 0x0088, 0x0888,
	0x0000, 0x0f00, 0x00f0, 0x0ff0,
	0x000f, 0x0f0f, 0x00ff, 0x0fff
};
static const uint16_t __not_in_flash("color_map") grays[16] = {
	0x0000, 0x0111, 0x0222, 0x0333,
	0x0444, 0x0555, 0x0666, 0x0777,
	0x0888, 0x0999, 0x0aaa, 0x0bbb,
	0x0ccc, 0x0ddd, 0x0eee, 0x0fff
};
#else
/* 565 colors and grays */
static const uint16_t __not_in_flash("color_map") colors[16] = {
	0x0000, 0x8000, 0x0400, 0x8400,
	0x0010, 0x8010, 0x0410, 0x8410,
	0x0000, 0xf800, 0x07e0, 0xffe0,
	0x001f, 0xf81f, 0x07ff, 0xffff
};
static const uint16_t __not_in_flash("color_map") grays[16] = {
	0x0000, 0x1082, 0x2104, 0x31a6,
	0x4228, 0x52aa, 0x632c, 0x73ae,
	0x8c51, 0x9cd3, 0xad55, 0xbdd7,
	0xce59, 0xdefb, 0xef7d, 0xffff
};
#endif

#define CROMEMCO_WIDTH	17
#define CROMEMCO_HEIGHT	132
static const uint8_t cromemco[] = {
	0x01, 0xf8, 0x00, 0x03, 0xfe, 0x00, 0x03, 0xff, 0x00, 0x03, 0xff, 0x00,
	0x07, 0x3f, 0x80, 0x06, 0x03, 0x80, 0x03, 0x01, 0x80, 0x03, 0xf1, 0x80,
	0x03, 0xff, 0x80, 0x01, 0xff, 0x80, 0x00, 0xff, 0x80, 0x00, 0x7f, 0x00,
	0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x03, 0xc2, 0x00,
	0x03, 0xe3, 0x00, 0x07, 0xc3, 0x00, 0x07, 0x03, 0x00, 0x07, 0x03, 0x80,
	0x03, 0x83, 0x80, 0x03, 0xff, 0x80, 0x01, 0xff, 0x80, 0x01, 0xff, 0x80,
	0x00, 0xff, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x03, 0xe3, 0x00, 0x07, 0xff, 0x00, 0x07, 0xff, 0x80,
	0x07, 0xff, 0x80, 0x03, 0xff, 0x80, 0x01, 0x00, 0x00, 0x01, 0x80, 0x00,
	0x03, 0xe0, 0x00, 0x07, 0xff, 0x00, 0x07, 0xff, 0x80, 0x07, 0xff, 0x80,
	0x03, 0xff, 0x80, 0x03, 0x87, 0x80, 0x01, 0x80, 0x00, 0x01, 0xc0, 0x00,
	0x07, 0xff, 0x00, 0x07, 0xff, 0x00, 0x03, 0xff, 0x80, 0x03, 0xff, 0x80,
	0x03, 0x0f, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x03, 0x80, 0x00, 0x03, 0xc6, 0x00, 0x07, 0xe7, 0x00, 0x07, 0xf3, 0x00,
	0x07, 0xf3, 0x00, 0x06, 0x33, 0x80, 0x03, 0x1b, 0x80, 0x03, 0xff, 0x80,
	0x01, 0xff, 0x80, 0x01, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x3e, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe3, 0x00,
	0x07, 0xff, 0x00, 0x07, 0xff, 0x80, 0x07, 0xff, 0x80, 0x03, 0xff, 0x80,
	0x03, 0x03, 0x00, 0x01, 0x80, 0x00, 0x01, 0xc0, 0x00, 0x07, 0xff, 0x00,
	0x07, 0xff, 0x00, 0x07, 0xff, 0x80, 0x03, 0xff, 0x80, 0x03, 0x8f, 0x80,
	0x01, 0x80, 0x00, 0x01, 0xc0, 0x00, 0x07, 0xfe, 0x00, 0x07, 0xff, 0x00,
	0x07, 0xff, 0x80, 0x03, 0xff, 0x80, 0x03, 0x1f, 0x80, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf8, 0x00, 0x03, 0xfe, 0x00,
	0x07, 0xfe, 0x00, 0x07, 0xff, 0x00, 0x07, 0x3f, 0x00, 0x06, 0x03, 0x80,
	0x07, 0x01, 0x80, 0x03, 0xe3, 0x80, 0x03, 0xff, 0x80, 0x01, 0xff, 0x80,
	0x01, 0xff, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00,
	0x07, 0xc0, 0x00, 0x07, 0xc0, 0x00, 0x07, 0x80, 0x00, 0x03, 0x80, 0x00,
	0x01, 0xf0, 0x00, 0x07, 0xff, 0x00, 0x07, 0xff, 0x00, 0x07, 0xff, 0x80,
	0x03, 0xff, 0x80, 0x03, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x18, 0x00, 0x7c, 0x1e, 0x00, 0x7c, 0x1e, 0x00,
	0x70, 0x0e, 0x00, 0xe0, 0x07, 0x00, 0xe0, 0x07, 0x00, 0xe0, 0x07, 0x80,
	0xf0, 0x07, 0x80, 0x7c, 0x0f, 0x80, 0x7f, 0xff, 0x80, 0x3f, 0xff, 0x80,
	0x3f, 0xff, 0x00, 0x1f, 0xff, 0x00, 0x07, 0xfe, 0x00, 0x03, 0xfc, 0x00
};

#define DAZZLER_WIDTH	21
#define DAZZLER_HEIGHT	130
static const uint8_t dazzler[] = {
	0x80, 0x00, 0x08, 0xc0, 0x00, 0x18, 0xe0, 0x00, 0x38, 0xff, 0xff, 0xf8,
	0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8,
	0xe0, 0x00, 0x18, 0xc0, 0x00, 0x18, 0xc0, 0x00, 0x18, 0xc0, 0x00, 0x18,
	0xc0, 0x00, 0x18, 0x60, 0x00, 0x38, 0x7c, 0x01, 0xf0, 0x3f, 0xff, 0xf0,
	0x1f, 0xff, 0xe0, 0x9f, 0xff, 0xe0, 0xcf, 0xff, 0xc0, 0xf3, 0xff, 0x00,
	0xfc, 0x18, 0x00, 0xff, 0x80, 0x00, 0xc3, 0xe0, 0x00, 0x81, 0xfc, 0x00,
	0x01, 0x1f, 0x80, 0x01, 0x03, 0xf0, 0x01, 0x0f, 0xf8, 0x01, 0xff, 0xf8,
	0x83, 0xff, 0xf8, 0xff, 0xff, 0xe0, 0xff, 0xff, 0x00, 0xff, 0xf8, 0x00,
	0xff, 0xc0, 0x00, 0xfe, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xc0, 0x00, 0x00,
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xe0, 0x03, 0xf8,
	0xf8, 0x01, 0xf8, 0xfc, 0x00, 0x78, 0xff, 0x00, 0x38, 0xff, 0xc0, 0x18,
	0xff, 0xf0, 0x18, 0xdf, 0xfc, 0x18, 0xc7, 0xff, 0x18, 0xc3, 0xff, 0x98,
	0xc0, 0xff, 0xf8, 0xc0, 0x3f, 0xf8, 0xc0, 0x1f, 0xf8, 0xe0, 0x07, 0xf8,
	0xf8, 0x01, 0xf8, 0xfc, 0x00, 0x78, 0xfc, 0x00, 0x38, 0x00, 0x00, 0x00,
	0x80, 0x03, 0xf8, 0xe0, 0x01, 0xf8, 0xf8, 0x00, 0x78, 0xfe, 0x00, 0x38,
	0xff, 0x80, 0x18, 0xff, 0xe0, 0x18, 0xff, 0xf8, 0x18, 0xdf, 0xfe, 0x18,
	0xc7, 0xff, 0x18, 0xc1, 0xff, 0xd8, 0xc0, 0x7f, 0xf8, 0xc0, 0x3f, 0xf8,
	0xc0, 0x0f, 0xf8, 0xe0, 0x03, 0xf8, 0xf8, 0x00, 0xf8, 0xfc, 0x00, 0x78,
	0xfc, 0x00, 0x18, 0x00, 0x00, 0x00, 0x80, 0x00, 0x08, 0xc0, 0x00, 0x38,
	0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8,
	0xff, 0xff, 0xf8, 0xc0, 0x00, 0x18, 0xc0, 0x00, 0x18, 0xc0, 0x00, 0x08,
	0xc0, 0x00, 0x00, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x00, 0xe0, 0x00, 0x00,
	0xfc, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x06, 0x00, 0x00, 0x80, 0x00, 0x18,
	0xc0, 0x00, 0x38, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8,
	0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xc0, 0x20, 0x18, 0xc0, 0x20, 0x18,
	0xc0, 0x20, 0x18, 0xc0, 0x70, 0x18, 0xc1, 0xfc, 0x18, 0xc1, 0xfc, 0x18,
	0xe0, 0x00, 0x78, 0xf8, 0x03, 0xf8, 0xfc, 0x01, 0xf8, 0x1c, 0x00, 0x00,
	0x80, 0x00, 0x08, 0xc0, 0x00, 0x18, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8,
	0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xf8,
	0xc0, 0x20, 0x08, 0xc0, 0xe0, 0x08, 0x41, 0xf0, 0x18, 0x07, 0xf0, 0x18,
	0x1f, 0xfe, 0x78, 0x7f, 0xff, 0xf8, 0xff, 0x8f, 0xf8, 0xff, 0x0f, 0xf0,
	0xfc, 0x07, 0xf0, 0xf8, 0x03, 0xc0, 0xf0, 0x00, 0x00, 0xc0, 0x00, 0x00,
	0xc0, 0x00, 0x00, 0x80, 0x00, 0x00
};

/* DAZZLER stuff */
static int state;
static WORD dma_addr;
static BYTE flags = 64;
static BYTE format;

/* centered image on 240x135 LCD */
#define XOFF	56
#define YOFF	3

static inline void pixel(uint16_t x, uint16_t y, uint16_t color)
{
	Paint_FastPixel(XOFF + x, YOFF + y, color);
}

/* draw pixels for one frame in hires */
static void __not_in_flash_func(draw_hires)(void)
{
	int x, y, i, j, c;
	WORD addr = dma_addr;
	unsigned int cmap[2];
	unsigned int c0, c1, c2, c3, c4, c5, c6, c7;

	/* set color or grayscale from lower nibble in graphics format */
	c = format & 0x0f;
	cmap[0] = BLACK;
	cmap[1] = (format & 16) ? colors[c] : grays[c];

	if (format & 32) {	/* 2048 bytes memory */
		for (j = 0; j < 128; j += 64) {
			for (i = 0; i < 128; i += 64) {
				for (y = j; y < j + 64; y += 2) {
					for (x = i; x < i + 64;) {
						c = dma_read(addr++);
						c0 = cmap[c & 1];
						c1 = cmap[(c >> 1) & 1];
						c2 = cmap[(c >> 2) & 1];
						c3 = cmap[(c >> 3) & 1];
						c4 = cmap[(c >> 4) & 1];
						c5 = cmap[(c >> 5) & 1];
						c6 = cmap[(c >> 6) & 1];
						c7 = cmap[(c >> 7) & 1];
						pixel(x, y, c0);
						pixel(x + 1, y, c1);
						pixel(x, y + 1, c2);
						pixel(x + 1, y + 1, c3);
						x += 2;
						pixel(x, y, c4);
						pixel(x + 1, y, c5);
						pixel(x, y + 1, c6);
						pixel(x + 1, y + 1, c7);
						x += 2;
					}
				}
			}
		}
	} else {		/* 512 bytes memory */
		for (j = 0; j < 128; j += 4) {
			for (i = 0; i < 128; i += 8) {
				c = dma_read(addr++);
				c0 = cmap[c & 1];
				c1 = cmap[(c >> 1) & 1];
				c2 = cmap[(c >> 2) & 1];
				c3 = cmap[(c >> 3) & 1];
				c4 = cmap[(c >> 4) & 1];
				c5 = cmap[(c >> 5) & 1];
				c6 = cmap[(c >> 6) & 1];
				c7 = cmap[(c >> 7) & 1];
				for (y = j; y < j + 4; y += 2) {
					for (x = i; x < i + 8;) {
						pixel(x, y, c0);
						pixel(x + 1, y, c1);
						pixel(x, y + 1, c2);
						pixel(x + 1, y + 1, c3);
						x += 2;
						pixel(x, y, c4);
						pixel(x + 1, y, c5);
						pixel(x, y + 1, c6);
						pixel(x + 1, y + 1, c7);
						x += 2;
					}
				}
			}
		}
	}
}

/* draw pixels for one frame in lowres */
static void __not_in_flash_func(draw_lowres)(void)
{
	int x, y, i, j, c;
	WORD addr = dma_addr;
	const uint16_t *cmap;
	unsigned int c0, c1;

	cmap = (format & 16) ? colors : grays;
	/* get size of DMA memory and draw the pixels */
	if (format & 32) {	/* 2048 bytes memory */
		for (j = 0; j < 128; j += 64) {
			for (i = 0; i < 128; i += 64) {
				for (y = j; y < j + 64; y += 2) {
					for (x = i; x < i + 64;) {
						c = dma_read(addr++);
						c0 = cmap[c & 0x0f];
						c1 = cmap[(c >> 4) & 0x0f];
						pixel(x, y, c0);
						pixel(x + 1, y, c0);
						pixel(x, y + 1, c0);
						pixel(x + 1, y + 1, c0);
						x += 2;
						pixel(x, y, c1);
						pixel(x + 1, y, c1);
						pixel(x, y + 1, c1);
						pixel(x + 1, y + 1, c1);
						x += 2;
					}
				}
			}
		}
	} else {		/* 512 bytes memory */
		for (j = 0; j < 128; j += 4) {
			for (i = 0; i < 128; i += 8) {
				c = dma_read(addr++);
				c0 = cmap[c & 0x0f];
				c1 = cmap[(c >> 4) & 0x0f];
				for (y = j; y < j + 4; y += 2) {
					for (x = i; x < i + 8;) {
						pixel(x, y, c0);
						pixel(x + 1, y, c0);
						pixel(x, y + 1, c0);
						pixel(x + 1, y + 1, c0);
						x += 2;
						pixel(x, y, c1);
						pixel(x + 1, y, c1);
						pixel(x, y + 1, c1);
						pixel(x + 1, y + 1, c1);
						x += 2;
					}
				}
			}
		}
	}
}

static void __no_inline_not_in_flash_func(draw_bitmap)(const uint8_t *bitmap,
						       int width, int height,
						       int x, int y)
{
	int i, j;
	uint8_t m;

	for (j = 0; j < height; j++) {
		m = 0x80;
		for (i = 0; i < width; i++) {
			if (*bitmap & m)
				Paint_FastPixel(x, y, BRRED);
			if ((m >>= 1) == 0) {
				m = 0x80;
				bitmap++;
			}
			x++;
		}
		if (width & 7)
			bitmap++;
		x -= width;
		y++;
	}
}

static void __not_in_flash_func(dazzler_draw)(int first_flag)
{
	if (first_flag) {
		Paint_Clear(BLACK);
		draw_bitmap(cromemco, CROMEMCO_WIDTH, CROMEMCO_HEIGHT, 14, 1);
		draw_bitmap(dazzler, DAZZLER_WIDTH, DAZZLER_HEIGHT, 209, 2);
		return;
	}

	if (format & 64)
		draw_hires();
	else
		draw_lowres();

	/* frame done, set frame flag for 4ms */
	flags = 0;
	sleep_ms(4);
	flags = 64;
}

void dazzler_ctl_out(BYTE data)
{
	/* get DMA address for display memory */
	dma_addr = (data & 0x7f) << 9;

	/* switch DAZZLER on/off */
	if (data & 128) {
		if (state == 0) {
			state = 1;
			lcd_custom_disp(dazzler_draw);
		}
	} else {
		if (state == 1) {
			state = 0;
			lcd_status_disp();
		}
	}
}

BYTE dazzler_flags_in(void)
{
	return flags;
}

void dazzler_format_out(BYTE data)
{
	format = data;
}
