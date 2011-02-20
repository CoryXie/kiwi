/*
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		PC Programmable Interval Timer code.
 */

#include <arch/io.h>
#include <cpu/intr.h>
#include <pc/pit.h>

/** Handle a PIT tick.
 * @param num		IRQ number.
 * @param data		Data associated with IRQ (unused).
 * @param frame		Interrupt stack frame.
 * @return		IRQ status code. */
static irq_result_t pit_handler(unative_t num, void *data, intr_frame_t *frame) {
	return (timer_tick()) ? IRQ_PREEMPT : IRQ_HANDLED;
}

/** Enable the PIT. */
static void pit_enable(void) {
	uint16_t base;

	/* Set frequency (1000Hz) */
	base = 1193182L / PIT_FREQUENCY;
	out8(0x43, 0x36);
	out8(0x40, base & 0xFF);
	out8(0x40, base >> 8);

	irq_register(0, pit_handler, NULL, NULL);
}

/** Disable the PIT. */
static void pit_disable(void) {
	irq_unregister(0, pit_handler, NULL, NULL);
}

/** PIT clock source. */
timer_device_t pit_timer_device = {
	.name = "PIT",
	.type = TIMER_DEVICE_PERIODIC,
	.enable = pit_enable,
	.disable = pit_disable,
};
