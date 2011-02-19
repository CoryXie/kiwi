/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 local APIC code.
 */

#include <arch/x86/lapic.h>
#include <arch/io.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <kargs.h>
#include <kdbg.h>
#include <time.h>

extern void ipi_process_pending(void);

/** Local APIC mapping. If NULL the LAPIC is not present. */
static volatile uint32_t *lapic_mapping = NULL;

/** Read from a register in the current CPU's local APIC.
 * @param reg		Register to read from.
 * @return		Value read from register. */
static inline uint32_t lapic_read(int reg) {
	return lapic_mapping[reg];
}

/** Write to a register in the current CPU's local APIC.
 * @param reg		Register to write to.
 * @param value		Value to write to register. */
static inline void lapic_write(int reg, uint32_t value) {
	lapic_mapping[reg] = value;
}

/** Send an EOI to the local APIC. */
static inline void lapic_eoi(void) {
	lapic_write(LAPIC_REG_EOI, 0);
}

/** Spurious interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame. */
static void lapic_spurious_handler(unative_t num, intr_frame_t *frame) {
	kprintf(LOG_DEBUG, "lapic: received spurious interrupt\n");
}

/** IPI message interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame. */
static void lapic_ipi_handler(unative_t num, intr_frame_t *frame) {
	ipi_process_pending();
	lapic_eoi();
}

/** Enable the local APIC timer. */
static void lapic_timer_enable(void) {
	/* Set the interrupt vector, no extra bits = Unmasked/One-shot. */
	lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_VECT_TIMER);
}

/** Disable the local APIC timer. */
static void lapic_timer_disable(void) {
	/* Set bit 16 in the Timer LVT register to 1 (Masked) */
	lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_VECT_TIMER | (1<<16));
}

/** Prepare local APIC timer tick.
 * @param us		Number of microseconds to tick in. */
static void lapic_timer_prepare(useconds_t us) {
	uint32_t count = (uint32_t)((curr_cpu->arch.lapic_timer_cv * us) >> 32);
	lapic_write(LAPIC_REG_TIMER_INITIAL, (count == 0 && us != 0) ? 1 : count);
}

/** Local APIC timer device. */
static timer_device_t lapic_timer_device = {
	.name = "LAPIC",
	.type = TIMER_DEVICE_ONESHOT,
	.enable = lapic_timer_enable,
	.disable = lapic_timer_disable,
	.prepare = lapic_timer_prepare,
};

/** Timer interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame. */
static void lapic_timer_handler(unative_t num, intr_frame_t *frame) {
	curr_cpu->should_preempt = timer_tick();
	lapic_eoi();
}

/** Get the current local APIC ID.
 * @return		Local APIC ID. */
uint32_t lapic_id(void) {
	if(!lapic_mapping) {
		return 0;
	}
	return (lapic_read(LAPIC_REG_APIC_ID) >> 24);
}

/** Send an IPI.
 * @param dest		Destination Shorthand.
 * @param id		Destination local APIC ID (if APIC_IPI_DEST_SINGLE).
 * @param mode		Delivery Mode.
 * @param vector	Value of vector field. */
void lapic_ipi(uint8_t dest, uint8_t id, uint8_t mode, uint8_t vector) {
	bool state;

	/* Must perform this check to prevent problems if fatal() is called
	 * before we've initialised the LAPIC. */
	if(!lapic_mapping) {
		return;
	}

	state = intr_disable();

	/* Write the destination ID to the high part of the ICR. */
	lapic_write(LAPIC_REG_ICR1, ((uint32_t)id << 24));

	/* Send the IPI:
	 * - Destination Mode: Physical.
	 * - Level: Assert (bit 14).
	 * - Trigger Mode: Edge. */
	lapic_write(LAPIC_REG_ICR0, (1<<14) | (dest << 18) | (mode << 8) | vector);

	/* Wait for the IPI to be sent (check Delivery Status bit). */
	while(lapic_read(LAPIC_REG_ICR0) & (1<<12)) {
		__asm__ volatile("pause");
	}

	intr_restore(state);
}

/** Send an IPI interrupt to a single CPU.
 * @param dest		Destination CPU ID. */
void ipi_arch_interrupt(cpu_id_t dest) {
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, (uint32_t)dest, LAPIC_IPI_FIXED, LAPIC_VECT_IPI);
}

/** Initialise the local APIC on the current CPU.
 * @param args		Kernel arguments structure. */
__init_text void lapic_init(kernel_args_t *args) {
	/* Don't do anything if the bootloader did not detect an LAPIC or if
	 * it was manually disabled. */
	if(args->arch.lapic_disabled) {
		return;
	}

	/* If the mapping has not been made, we're being run on the BSP. Create
	 * it and register interrupt vector handlers. */
	if(!lapic_mapping) {
		lapic_mapping = phys_map(args->arch.lapic_address, PAGE_SIZE, MM_FATAL);
		kprintf(LOG_NORMAL, "lapic: physical location 0x%" PRIpp ", mapped to %p\n",
		        args->arch.lapic_address, lapic_mapping);

		intr_register(LAPIC_VECT_SPURIOUS, lapic_spurious_handler);
		intr_register(LAPIC_VECT_TIMER, lapic_timer_handler);
		intr_register(LAPIC_VECT_IPI, lapic_ipi_handler);
	}

	/* Enable the local APIC (bit 8) and set the spurious interrupt
	 * vector in the Spurious Interrupt Vector Register. */
	lapic_write(LAPIC_REG_SPURIOUS, LAPIC_VECT_SPURIOUS | (1<<8));
	lapic_write(LAPIC_REG_TIMER_DIVIDER, LAPIC_TIMER_DIV8);

	/* Accept all interrupts. */
	lapic_write(LAPIC_REG_TPR, lapic_read(LAPIC_REG_TPR & 0xFFFFFF00));

	/* Figure out the timer conversion factor. */
	curr_cpu->arch.lapic_timer_cv = ((curr_cpu->arch.lapic_freq / 8) << 32) / 1000000;
	kprintf(LOG_NORMAL, "lapic: timer conversion factor for CPU %u is %u\n",
	        curr_cpu->id, curr_cpu->arch.lapic_timer_cv);

	/* Set the timer device. */
	timer_device_set(&lapic_timer_device);
}
