/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2025 by Thomas Eberhardt
 */

#ifndef GPIO_INC
#define GPIO_INC

/*
 * Additional hardware defines that are not in the board files
 */

#ifndef WAVESHARE_I2CADC_I2C
#define WAVESHARE_I2CADC_I2C 0
#endif
#ifndef WAVESHARE_I2CADC_SDA_PIN
#define WAVESHARE_I2CADC_SDA_PIN 28
#endif
#ifndef WAVESHARE_I2CADC_SCL_PIN
#define WAVESHARE_I2CADC_SCL_PIN 29
#endif

#ifndef WAVESHARE_LCD_WIDTH
#define WAVESHARE_LCD_WIDTH 240
#endif
#ifndef WAVESHARE_LCD_HEIGHT
#define WAVESHARE_LCD_HEIGHT 135
#endif

#endif /* !GPIO_INC */
