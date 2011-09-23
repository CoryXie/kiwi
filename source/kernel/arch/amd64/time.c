/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		AMD64 time handling functions.
 */

#include <x86/cpu.h>
#include <x86/tsc.h>

#include <cpu/cpu.h>

#include <time.h>

/** Get the system time (number of microseconds since boot).
 * @return		Number of microseconds since system was booted. */
useconds_t system_time(void) {
	return (useconds_t)((x86_rdtsc() - curr_cpu->arch.system_time_offset) / curr_cpu->arch.cycles_per_us);
}

/** Set up the boot time offset. */
__init_text void tsc_init(void) {
	/* Calculate the offset to subtract from the TSC when calculating the
	 * system time. For the boot CPU, this is the current value of the TSC,
	 * so the system time at this point is 0. For other CPUs, we need to
	 * synchronise against the boot CPU so system_time() reads the same
	 * value on all CPUs. */
	//if(curr_cpu == &boot_cpu) {
		curr_cpu->arch.system_time_offset = x86_rdtsc();
	//} else {
		// TODO
	//}
}
