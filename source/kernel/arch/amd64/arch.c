/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		AMD64 architecture initialisation code.
 */

#include <x86/cpu.h>
#include <x86/lapic.h>

#include <cpu/cpu.h>

#include <console.h>
#include <kernel.h>
#include <time.h>

#if CONFIG_SMP
#if 0
/** x86-specific initialisation for an AP.
 * @param cpu		CPU structure for the AP. */
__init_text void arch_ap_init(cpu_t *cpu) {
	cpu_arch_init(cpu);
	pat_init();
	lapic_init();
	syscall_arch_init();
}
#endif
#endif
