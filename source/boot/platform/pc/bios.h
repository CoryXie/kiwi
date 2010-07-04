/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		BIOS interrupt functions.
 */

#ifndef __PLATFORM_BIOS_H
#define __PLATFORM_BIOS_H

#include <arch/cpu.h>
#include <lib/string.h>
#include <types.h>

/** Memory area to use when passing data to BIOS interrupts (56KB).
 * @note		Area is actually 60KB, but the last 4KB are used for
 *			the stack. */
#define BIOS_MEM_BASE		0x1000
#define BIOS_MEM_SIZE		0xE000

/** Convert a segment + offset pair to a linear address. */
#define SEGOFF2LIN(segoff)	(ptr_t)((((segoff) & 0xFFFF0000) >> 12) + ((segoff) & 0xFFFF))

/** Convert a linear address to a segment + offset pair. */
#define LIN2SEGOFF(lin)		(uint32_t)(((lin & 0xFFFFFFF0) << 12) + (lin & 0xF))

/** Structure describing registers to pass to a BIOS interrupt. */
typedef struct bios_regs {
	uint32_t eflags, eax, ebx, ecx, edx, edi, esi, ebp, es;
} bios_regs_t;

/** Initialise a BIOS registers structure.
 * @param regs		Structure to initialise. */
static inline void bios_regs_init(bios_regs_t *regs) {
	memset(regs, 0, sizeof(bios_regs_t));
}

extern void bios_interrupt(uint8_t num, bios_regs_t *regs);

#endif /* __PLATFORM_BIOS_H */