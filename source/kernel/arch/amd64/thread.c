/* Kiwi AMD64 thread functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		AMD64 thread functions.
 */

#include <arch/x86/sysreg.h>

#include <cpu/cpu.h>

#include <proc/sched.h>
#include <proc/thread.h>

/** AMD64-specific post-thread switch function. */
void thread_arch_post_switch(thread_t *thread) {
	/* Set the RSP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	thread->cpu->arch.tss.rsp0 = (ptr_t)thread->kstack + KSTACK_SIZE;

	/* Store the address of the thread's architecture data in the
	 * KERNEL_GS_BASE MSR for the SYSCALL handler to use. */
        sysreg_msr_write(SYSREG_MSR_K_GS_BASE, (ptr_t)&thread->arch);
}

/** Initialize AMD64-specific thread data.
 * @param thread	Thread to initialize.
 * @return		Always returns 0. */
int thread_arch_init(thread_t *thread) {
	thread->arch.kernel_rsp = (ptr_t)thread->kstack + KSTACK_SIZE;
	thread->arch.user_rsp = 0;
	return 0;
}

/** Clean up AMD64-specific thread data.
 * @param thread	Thread to clean up. */
void thread_arch_destroy(thread_t *thread) {
	/* Nothing happens. */
}
