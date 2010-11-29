/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		PC BIOS interrupt interface.
 */

#ifndef __PLATFORM_PC_BIOS_H
#define __PLATFORM_PC_BIOS_H

#include <arch/x86/cpu.h>
#include <mm/flags.h>

/** Convert a segment + offset pair to a linear address. */
#define SEGOFF2LIN(segoff)	((ptr_t)(((segoff) & 0xFFFF0000) >> 12) + ((segoff) & 0xFFFF))

/** Structure describing registers to pass to a BIOS interrupt. */
typedef struct bios_regs {
	uint32_t eax, ebx, ecx, edx, edi, esi, ebp, eflags;
	uint32_t ds, es, fs, gs;
} bios_regs_t;

extern void *bios_mem_alloc(size_t size, int mmflag);
extern void bios_mem_free(void *addr, size_t size);
extern uint32_t bios_mem_virt2phys(void *addr);
extern void *bios_mem_phys2virt(uint32_t addr);

extern void bios_regs_init(bios_regs_t *regs);

extern void bios_interrupt(uint8_t num, bios_regs_t *regs);

#endif /* __PLATFORM_PC_BIOS_H */