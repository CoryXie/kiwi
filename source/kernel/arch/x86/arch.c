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
 * @brief		x86 architecture core code.
 */

#include <arch/arch.h>
#include <arch/io.h>
#include <arch/lapic.h>
#include <arch/page.h>
#include <arch/syscall.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <console.h>
#include <fatal.h>

/** x86-specific early initialisation.
 * @param data		Multiboot information pointer. */
void __init_text arch_premm_init(void *data) {
	descriptor_init();
	intr_init();
	cpu_arch_init(&curr_cpu->arch);
}

/** x86-specific second stage initialisation. */
void __init_text arch_postmm_init(void) {
	lapic_init();
	syscall_arch_init();
}

/** x86-specific final initialisation. */
void __init_text arch_final_init(void) {
	page_late_init();
}

/** x86-specific initialisation for an AP. */
void __init_text arch_ap_init(void) {
	descriptor_ap_init();
	cpu_arch_init(&curr_cpu->arch);

	/* Initialise the LAPIC. */
	if(!lapic_init()) {
		fatal("LAPIC initialisation failed for CPU %" PRIu32 "\n", curr_cpu->id);
	}
#ifdef __x86_64__
	syscall_arch_init();
#endif
}

/** Reboot the system. */
void arch_reboot(void) {
	int i;

	/* Try the keyboard controller. */
	out8(0x64, 0xfe);
	for(i = 0; i < 10000000; i++) {
		__asm__ volatile("pause");
	}

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}