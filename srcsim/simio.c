/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024-2025 by Udo Munk & Thomas Eberhardt
 *
 * I/O simulation for picosim
 *
 * History:
 * 28-APR-2024 all I/O implemented for a first release
 * 09-MAY-2024 improved so that it can run some MITS Altair software
 * 11-MAY-2024 allow us to configure the port 255 value
 * 27-MAY-2024 moved io_in & io_out to simcore
 * 31-MAY-2024 added hardware control port for z80pack machines
 * 08-JUN-2024 implemented system reset
 * 09-JUN-2024 implemented boot ROM
 * 24-JUN-2024 added emulation of Cromemco Dazzler
 * 29-JUN-2024 implemented banked memory
 */

/* Raspberry SDK includes */
#include <stdio.h>
#if LIB_PICO_STDIO_USB || (LIB_STDIO_MSC_USB && !STDIO_MSC_USB_DISABLE_STDIO)
#include <tusb.h>
#endif
#include "pico/stdlib.h"
#include "hardware/uart.h"

/* Project includes */
#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simmem.h"
#include "simcore.h"
#include "simio.h"

#include "dazzler.h"
#include "draw.h"
#include "lcd.h"
#include "rtc80.h"
#include "sd-fdc.h"

/*
 *	Forward declarations of the I/O functions
 *	for all port addresses.
 */
static void p000_out(BYTE data), p001_out(BYTE data), p254_out(BYTE data);
static void hwctl_out(BYTE data);
static BYTE p000_in(void), p001_in(void), p255_in(void), hwctl_in(void);
static void mmu_out(BYTE data), fp_out(BYTE data);
static BYTE mmu_in(void);

static BYTE sio_last;	/* last character received */
       BYTE fp_value;	/* port 255 value, can be set from ICE or config() */
static BYTE hwctl_lock = 0xff; /* lock status hardware control port */

/*
 *	This array contains function pointers for every input
 *	I/O port (0 - 255), to do the required I/O.
 */
in_func_t *const port_in[256] = {
	[  0] = p000_in,	/* SIO status */
	[  1] = p001_in,	/* SIO data */
	[  4] = fdc_in,		/* FDC status */
	[ 14] = dazzler_flags_in, /* Cromemco Dazzler flags */
	[ 64] = mmu_in,		/* MMU */
	[ 65] = clkc_in,	/* RTC read clock command */
	[ 66] = clkd_in,	/* RTC read clock data */
	[160] = hwctl_in,	/* virtual hardware control */
	[254] = p255_in,	/* mirror of port 255 */
	[255] = p255_in		/* read from front panel switches */
};

/*
 *	This array contains function pointers for every output
 *	I/O port (0 - 255), to do the required I/O.
 */
out_func_t *const port_out[256] = {
	[  0] = p000_out,	/* RGB LED */
	[  1] = p001_out,	/* SIO data */
	[  4] = fdc_out,	/* FDC command */
	[ 14] = dazzler_ctl_out, /* Cromemco Dazzler control */
	[ 15] = dazzler_format_out, /* Cromemco Dazzler format */
	[ 64] = mmu_out,	/* MMU */
	[ 65] = clkc_out,	/* RTC write clock command */
	[ 66] = clkd_out,	/* RTC write clock data */
	[160] = hwctl_out,	/* virtual hardware control */
	[254] = p254_out,	/* write to front panel switches */
	[255] = fp_out		/* write to front panel lights */
};

/*
 *	This function is to initiate the I/O devices.
 *	It will be called from the CPU simulation before
 *	any operation with the CPU is possible.
 */
void init_io(void)
{
}

/*
 *	I/O function port 0 read:
 *	read status of the Pico UART and return:
 *	bit 0 = 0, character available for input from tty
 *	bit 7 = 0, transmitter ready to write character to tty
 */
static BYTE p000_in(void)
{
	register BYTE stat = 0b10000001; /* initially not ready */

#if LIB_PICO_STDIO_UART
	uart_inst_t *my_uart = uart_default;

	if (uart_is_writable(my_uart))	/* check if output to UART is possible */
		stat &= 0b01111111;	/* if so flip status bit */
	if (uart_is_readable(my_uart))	/* check if there is input from UART */
		stat &= 0b11111110;	/* if so flip status bit */
#endif
#if LIB_PICO_STDIO_USB || (LIB_STDIO_MSC_USB && !STDIO_MSC_USB_DISABLE_STDIO)
	if (tud_cdc_connected()) {
		if (tud_cdc_write_available())	/* check if output to UART is possible */
			stat &= 0b01111111;	/* if so flip status bit */
		if (tud_cdc_available())	/* check if there is input from UART */
			stat &= 0b11111110;	/* if so flip status bit */
	}
#endif

	return stat;
}

/*
 *	I/O function port 1 read:
 *	Read byte from Pico UART.
 */
static BYTE p001_in(void)
{
	bool input_avail = false;

#if LIB_PICO_STDIO_UART
	uart_inst_t *my_uart = uart_default;

	if (uart_is_readable(my_uart))
		input_avail = true;
#endif
#if LIB_PICO_STDIO_USB || (LIB_STDIO_MSC_USB && !STDIO_MSC_USB_DISABLE_STDIO)
	if (tud_cdc_connected() && tud_cdc_available())
		input_avail = true;
#endif

	if (input_avail)
		sio_last = getchar();

	return sio_last;
}

/*
 *	Input from virtual hardware control port
 *	returns lock status of the port
 */
static BYTE hwctl_in(void)
{
	return hwctl_lock;
}

/*
 *	read MMU register
 */
static BYTE mmu_in(void)
{
	return selbnk;
}

/*
 *	I/O function port 255 read:
 *	return virtual front panel switches state
 */
static BYTE p255_in(void)
{
	return fp_value;
}

/*
 *	I/O function port 0 write:
 *	Switch RGB LED blue on/off.
 */
static void p000_out(BYTE data)
{
	if (!data) {
		/* 0 switches LED blue off */
		led_color &= ~C_BLUE;
	} else {
		/* everything else on */
		led_color = (led_color & ~C_BLUE) | C_BLUE;
	}
	lcd_update_led();
}

/*
 *	I/O function port 1 write:
 *	Write byte to Pico UART.
 */
static void p001_out(BYTE data)
{
	putchar_raw((int) data & 0x7f); /* strip parity, some software won't */
}

/*
 *	Port is locked until magic number 0xaa is received!
 *
 *	Virtual hardware control output.
 *	Used to shutdown and switch CPU's.
 *
 *	bit 3 = 1	select next LCD status display
 *	bit 4 = 1	switch CPU model to 8080
 *	bit 5 = 1	switch CPU model to Z80
 *	bit 6 = 1	reset system
 *	bit 7 = 1	halt emulation via I/O
 */
static void hwctl_out(BYTE data)
{
	/* if port is locked do nothing */
	if (hwctl_lock && (data != 0xaa))
		return;

	/* unlock port ? */
	if (hwctl_lock && (data == 0xaa)) {
		hwctl_lock = 0;
		return;
	}

	/* process output to unlocked port */
	/* but first lock port again */
	hwctl_lock = 0xff;

	if (data & 128) {
		cpu_error = IOHALT;
		cpu_state = ST_STOPPED;
		return;
	}

	if (data & 64) {
		reset_cpu();		/* reset CPU */
		reset_memory();		/* reset memory */
#ifdef SIMPLEPANEL
		fp_led_output = 0xff;	/* reset output fp LED's */
#endif
		PC = 0xff00;		/* power on jump to boot ROM */
		dazzler_ctl_out(0);	/* switch Dazzler off */
		return;
	}

#if !defined (EXCLUDE_I8080) && !defined(EXCLUDE_Z80)
	if (data & 32) {		/* switch cpu model to Z80 */
		switch_cpu(Z80);
		return;
	}

	if (data & 16) {		/* switch cpu model to 8080 */
		switch_cpu(I8080);
		return;
	}
#endif

	if (data & 8) {			/* select next LCD status display */
		lcd_status_next();
		return;
	}
}

/*
 *	write MMU register
 */
static void mmu_out(BYTE data)
{
	selbnk = data;
}

/*
 *	This allows to set the virtual front panel switches with ICE p command
 */
static void p254_out(BYTE data)
{
	fp_value = data;
}

/*
 *	Write output to front panel lights
 */
static void fp_out(BYTE data)
{
#ifdef SIMPLEPANEL
	fp_led_output = data;
#else
	UNUSED(data);
#endif
}
