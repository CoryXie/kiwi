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
 * @brief		Thread management code.
 */

#ifndef __PROC_THREAD_H
#define __PROC_THREAD_H

#include <arch/thread.h>

#include <cpu/context.h>
#include <cpu/cpu.h>
#include <cpu/fpu.h>

#include <kernel/signal.h>
#include <kernel/thread.h>

#include <lib/avl_tree.h>
#include <lib/list.h>
#include <lib/notifier.h>
#include <lib/refcount.h>

#include <sync/spinlock.h>

#include <object.h>
#include <time.h>

struct process;
struct waitq;

/** Entry function for a thread. */
typedef void (*thread_func_t)(void *, void *);

/** Thread creation arguments structure, for thread_uspace_trampoline(). */
typedef struct thread_uspace_args {
	ptr_t sp;			/**< Stack pointer. */
	ptr_t entry;			/**< Entry point address. */
	ptr_t arg;			/**< Argument. */
} thread_uspace_args_t;

/** Definition of a thread. */
typedef struct thread {
	object_t obj;			/**< Object header. */

	/** Main thread information. */
	spinlock_t lock;		/**< Protects the thread's internals. */
	context_t context;		/**< CPU context. */
	fpu_context_t *fpu;		/**< FPU context. */
	thread_arch_t arch;		/**< Architecture thread data. */
	unative_t *kstack;		/**< Kernel stack pointer. */
	int flags;			/**< Flags for the thread. */
	int priority;			/**< Priority of the thread. */
	size_t wire_count;		/**< How many calls to thread_wire() have been made. */
	refcount_t count;		/**< Number of handles to the thread. */
	bool killed;			/**< Whether thread_kill() has been called on the thread. */
	ptr_t ustack;			/**< User-mode stack base. */
	size_t ustack_size;		/**< Size of the user-mode stack. */

	/** Scheduling information. */
	list_t runq_link;		/**< Link to run queues. */
	int max_prio;			/**< Maximum scheduling priority. */
	int curr_prio;			/**< Current scheduling priority. */
	cpu_t *cpu;			/**< CPU that the thread runs on. */
	useconds_t timeslice;		/**< Current timeslice. */
	size_t preempt_off;		/**< Whether preemption is disabled. */
	bool preempt_missed;		/**< Whether preemption was missed due to being disabled. */

	/** Sleeping information. */
	list_t waitq_link;		/**< Link to wait queue. */
	struct waitq *waitq;		/**< Wait queue that the thread is sleeping on. */
	bool interruptible;		/**< Whether the sleep can be interrupted. */
	context_t sleep_context;	/**< Context to restore upon sleep interruption/timeout. */
	timer_t sleep_timer;		/**< Timer for sleep timeout. */
	bool timed_out;			/**< Whether the sleep timed out. */
	bool rwlock_writer;		/**< Whether the thread wants exclusive access to an rwlock. */

	/** Signal information. */
	sigset_t signal_mask;		/**< Signal mask for the thread. */
	sigset_t pending_signals;	/**< Bitmap of pending signals. */
	siginfo_t signal_info[NSIG];	/**< Information associated with pending signals. */
	stack_t signal_stack;		/**< Alternate signal stack. */

	/** Accounting information. */
	useconds_t last_time;		/**< Time that the thread entered/left the kernel. */
	useconds_t kernel_time;		/**< Total time the thread has spent in the kernel. */
	useconds_t user_time;		/**< Total time the thread has spent in user mode. */

	/** State of the thread. */
	enum {
		THREAD_CREATED,		/**< Thread is newly created. */
		THREAD_READY,		/**< Thread is runnable. */
		THREAD_RUNNING,		/**< Thread is running on a CPU. */
		THREAD_SLEEPING,	/**< Thread is sleeping. */
		THREAD_DEAD,		/**< Thread is dead and awaiting cleanup. */
	} state;

	/** Information used by user memory functions. */
	bool in_usermem;		/**< Whether the thread is in the user memory access functions. */
	context_t usermem_context;	/**< Context to restore upon user memory access fault. */

	/** Thread entry function. */
	thread_func_t entry;		/**< Entry function for the thread. */
	void *arg1;			/**< First argument to thread entry function. */
	void *arg2;			/**< Second argument to thread entry function. */

	/** Other thread information. */
	thread_id_t id;			/**< ID of the thread. */
	avl_tree_node_t tree_link;	/**< Link to thread tree. */
	char name[THREAD_NAME_MAX];	/**< Name of the thread. */
	notifier_t death_notifier;	/**< Notifier for thread death. */
	int status;			/**< Exit status of the thread. */
	struct process *owner;		/**< Pointer to parent process. */
	list_t owner_link;		/**< Link to parent process. */
} thread_t;

/** Macro that expands to a pointer to the current thread. */
#define curr_thread		(curr_cpu->thread)

extern void thread_arch_post_switch(thread_t *thread);
extern status_t thread_arch_init(thread_t *thread);
extern void thread_arch_destroy(thread_t *thread);
extern ptr_t thread_arch_tls_addr(thread_t *thread);
extern status_t thread_arch_set_tls_addr(thread_t *thread, ptr_t addr);
extern void thread_arch_enter_userspace(ptr_t entry, ptr_t stack, ptr_t arg) __noreturn;

extern void thread_uspace_trampoline(void *_args, void *arg2);

extern void thread_wire(thread_t *thread);
extern void thread_unwire(thread_t *thread);
extern bool thread_interrupt(thread_t *thread);
extern void thread_kill(thread_t *thread);
extern void thread_rename(thread_t *thread, const char *name);
extern void thread_at_kernel_entry(void);
extern void thread_at_kernel_exit(void);
extern void thread_exit(void) __noreturn;

extern thread_t *thread_lookup_unsafe(thread_id_t id);
extern thread_t *thread_lookup(thread_id_t id);
extern status_t thread_create(const char *name, struct process *owner, int flags,
                              thread_func_t entry, void *arg1, void *arg2,
                              object_security_t *security, thread_t **threadp);
extern void thread_run(thread_t *thread);
extern void thread_destroy(thread_t *thread);

extern int kdbg_cmd_kill(int argc, char **argv);
extern int kdbg_cmd_thread(int argc, char **argv);

extern void thread_init(void);
extern void thread_reaper_init(void);

#endif /* __PROC_THREAD_H */
