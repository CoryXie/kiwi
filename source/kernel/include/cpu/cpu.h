/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		CPU management.
 */

#ifndef __CPU_CPU_H
#define __CPU_CPU_H

#include <arch/cpu.h>
#include <lib/list.h>
#include <sync/spinlock.h>

struct kernel_args;
struct kernel_args_cpu_arch;
struct sched_cpu;
struct thread;
struct vm_aspace;

/** Structure describing a CPU. */
typedef struct cpu {
	list_t header;			/**< Link to running CPUs list. */

	cpu_id_t id;			/**< ID of the CPU. */
	cpu_arch_t arch;		/**< Architecture-specific information. */

	/** Scheduler information. */
	struct sched_cpu *sched;	/**< Scheduler run queues/timers. */
	struct thread *thread;		/**< Currently executing thread. */
	struct vm_aspace *aspace;	/**< Address space currently in use. */
	bool should_preempt;		/**< Whether the CPU should be preempted. */
	bool idle;			/**< Whether the CPU is idle. */

	/** IPI information. */
	list_t ipi_queue;		/**< List of IPI messages sent to this CPU. */
	bool ipi_sent;			/**< Whether it is necessary to send an IPI. */
	spinlock_t ipi_lock;		/**< Lock to protect IPI queue. */

	/** Timer information. */
	list_t timers;			/**< List of active timers. */
	spinlock_t timer_lock;		/**< Timer list lock. */
} cpu_t;

/** Expands to a pointer to the CPU structure of the current CPU. */
#define curr_cpu	((cpu_t *)cpu_get_pointer())

extern cpu_t boot_cpu;
extern size_t cpu_id_max;
extern size_t cpu_count;
extern list_t cpus_running;
extern cpu_t **cpus;

extern void cpu_pause_all(void);
extern void cpu_resume_all(void);
extern void cpu_halt_all(void);

extern cpu_id_t cpu_current_id(void);

extern void cpu_arch_init(cpu_t *cpu, struct kernel_args_cpu_arch *args);
extern void cpu_init(struct kernel_args *args);
extern void cpu_early_init(struct kernel_args *args);

extern int kdbg_cmd_cpus(int argc, char **argv);

#endif /* __CPU_CPU_H */
