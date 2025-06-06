/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2025 by Udo Munk & Thomas Eberhardt
 *
 * This module implements a TX UART on the DEBUG port, GP 2 is TX,
 * and stdio compatible functions debug_* to print on this port.
 *
 * History:
 * 06-JUN-2025 implemented first release of Z80 emulation
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "uart_tx.pio.h"

#include "gpio.h"
#include "debug.h"

#define SERIAL_BAUD	115200	/* baud rate we use */

/* PIO and sm we use */
static PIO pio = pio1;
static uint sm;

/*
 * initialialize TX UART using PIO
 * must be called before output to the port
 */
void debug_init(void)
{
	/* get next unused one */
	sm = pio_claim_unused_sm(pio, true);

	/* setup the GPIO as TX UART */
	uint offset = pio_add_program(pio, &uart_tx_program);
	uart_tx_program_init(pio, sm, offset, WAVESHARE_DEBUG_TX_PIN, SERIAL_BAUD);
}

/*
 * print string to debug port followed by CR/LF, so works same as stdio puts
 */
void debug_puts(const char *s)
{
	uart_tx_program_puts(pio, sm, s);
	uart_tx_program_putc(pio, sm, '\r');
	uart_tx_program_putc(pio, sm, '\n');
}
