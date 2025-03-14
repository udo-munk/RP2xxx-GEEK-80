/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024 by Udo Munk
 * Copyright (C) 2024-2025 by Thomas Eberhardt
 *
 * This module implements memory management for the Z80/8080 CPU.
 *
 * History:
 * 23-APR-2024 derived from z80sim
 * 29-JUN-2024 implemented banked memory
 * 14-DEC-2024 added hardware breakpoint support
 * 12-MAR-2025 added more memory banks for RP2350
 */

#ifndef SIMMEM_INC
#define SIMMEM_INC

#include "sim.h"
#include "simdefs.h"
#ifdef WANT_ICE
#include "simice.h"
#endif

#if defined(SIMPLEPANEL) || defined(BUS_8080)
#include "simglb.h"
#endif

#if PICO_RP2350
#define NUMSEG 7
#else
#define NUMSEG 2
#endif
#define SEGSIZ 49152

extern BYTE memcom[65536 - SEGSIZ], membnk[NUMSEG][SEGSIZ];
extern BYTE selbnk;

extern void init_memory(void), reset_memory(void);

/* Last page in memory is ROM and write protected. Some software */
/* expects a ROM in upper memory, if not it will wrap arround to */
/* address 0, and destroys itself with testing RAM access. */

/*
 * memory access for the CPU cores
 */
static inline void memwrt(WORD addr, BYTE data)
{
#ifdef BUS_8080
	cpu_bus &= ~(CPU_M1 | CPU_WO | CPU_MEMR);
#endif

#ifdef SIMPLEPANEL
	fp_led_address = addr;
	fp_led_data = data;
#endif

#ifdef WANT_HB
	if (hb_flag && hb_addr == addr && (hb_mode & HB_WRITE))
		hb_trig = HB_WRITE;
#endif

	if (addr >= SEGSIZ) {
		if (addr < 0xff00)
			memcom[addr - SEGSIZ] = data;
	} else {
		membnk[selbnk][addr] = data;
	}
}

static inline BYTE memrdr(WORD addr)
{
	register BYTE data;

#ifdef WANT_HB
	if (hb_flag && hb_addr == addr) {
		if (cpu_bus & CPU_M1) {
			if (hb_mode & HB_EXEC)
				hb_trig = HB_EXEC;
		} else {
			if (hb_mode & HB_READ)
				hb_trig = HB_READ;
		}
	}
#endif

	if (addr >= SEGSIZ)
		data = memcom[addr - SEGSIZ];
	else
		data = membnk[selbnk][addr];

#ifdef BUS_8080
	cpu_bus &= ~CPU_M1;
	cpu_bus |= CPU_WO | CPU_MEMR;
#endif

#ifdef SIMPLEPANEL
	fp_led_address = addr;
	fp_led_data = data;
#endif

	return data;
}

/*
 * memory access for DMA devices which request bus from CPU
 */
static inline void dma_write(WORD addr, BYTE data)
{
	if (addr >= SEGSIZ) {
		if (addr < 0xff00)
			memcom[addr - SEGSIZ] = data;
	} else {
		membnk[selbnk][addr] = data;
	}
}

static inline BYTE dma_read(WORD addr)
{
	if (addr >= SEGSIZ)
		return memcom[addr - SEGSIZ];
	else
		return membnk[selbnk][addr];
}

/*
 * direct memory access for simulation frame, video logic, etc.
 */
static inline void putmem(WORD addr, BYTE data)
{
	if (addr >= SEGSIZ) {
		if (addr < 0xff00)
			memcom[addr - SEGSIZ] = data;
	} else {
		membnk[selbnk][addr] = data;
	}
}

static inline BYTE getmem(WORD addr)
{
	if (addr >= SEGSIZ)
		return memcom[addr - SEGSIZ];
	else
		return membnk[selbnk][addr];
}

#endif /* !SIMMEM_INC */
