/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Thread management code.
 */

#include <cpu/cpu.h>
#include <cpu/ipi.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>
#include <sync/waitq.h>

#include <types/avl.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);
extern void waitq_do_wake(thread_t *thread);

static AVL_TREE_DECLARE(thread_tree);		/**< Tree of all threads. */
static MUTEX_DECLARE(thread_tree_lock, 0);	/**< Lock for thread AVL tree. */
static vmem_t *thread_id_arena;			/**< Thread ID Vmem arena. */
static slab_cache_t *thread_cache;		/**< Cache for thread structures. */

/** Dead thread queue information. */
static LIST_DECLARE(dead_threads);
static SPINLOCK_DECLARE(dead_thread_lock);
static SEMAPHORE_DECLARE(dead_thread_sem, 0);

/** Constructor for thread objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int thread_cache_ctor(void *obj, void *data, int kmflag) {
	thread_t *thread = (thread_t *)obj;

	spinlock_init(&thread->lock, "thread_lock");
	list_init(&thread->header);
	list_init(&thread->waitq_link);
	list_init(&thread->owner_link);
	return 0;
}

/** Thread entry point.
 *
 * Entry point for all threads. Wraps the real main function for a thread to
 * peform post-switch tasks before calling the function. This is necessary
 * because when real_schedule() switches to a newly-created thread, it will
 * return to this function and sched_post() does not get called by the
 * scheduler, so this function must do that.
 */
static void thread_trampoline(void) {
	sched_post_switch(true);

	dprintf("thread: entered thread %" PRId32 "(%s) on CPU %" PRIu32 "\n",
		curr_thread->id, curr_thread->name, curr_cpu->id);

	curr_thread->entry(curr_thread->arg1, curr_thread->arg2);
	thread_exit();
	while(1) {
		sched_yield();
	}
}

/** Dead thread reaper.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void thread_reaper(void *arg1, void *arg2) {
	thread_t *thread;

	while(true) {
		semaphore_down(&dead_thread_sem, 0);

		/* Take the next thread off the list. */
		spinlock_lock(&dead_thread_lock, 0);
		assert(!list_empty(&dead_threads));
		thread = list_entry(dead_threads.next, thread_t, header);
		list_remove(&thread->header);
		spinlock_unlock(&dead_thread_lock);

		/* Remove from thread tree. */
		mutex_lock(&thread_tree_lock, 0);
		avl_tree_remove(&thread_tree, (key_t)thread->id);
		mutex_unlock(&thread_tree_lock);

		/* Detach from its owner. */
		process_detach(thread);

		/* Now clean up the thread. */
		kheap_free(thread->kstack, KSTACK_SIZE);
		context_destroy(&thread->context);
		thread_arch_destroy(thread);
		if(thread->fpu) {
			fpu_context_destroy(thread->fpu);
		}

		/* Deallocate the thread ID. */
		vmem_free(thread_id_arena, (vmem_resource_t)thread->id, 1);

		dprintf("thread: destroyed thread %" PRId32 "(%s) (thread: %p)\n",
			thread->id, thread->name, thread);

		slab_cache_free(thread_cache, thread);
	}
}

/** Run a newly-created thread.
 *
 * Moves a newly created thread into the Ready state and places it on the
 * run queue of the current CPU.
 *
 * @param thread	Thread to run.
 */
void thread_run(thread_t *thread) {
	spinlock_lock(&thread->lock, 0);

	assert(thread->state == THREAD_CREATED);

	thread->state = THREAD_READY;
	thread->cpu = curr_cpu;
	sched_thread_insert(thread);

	spinlock_unlock(&thread->lock);
}

/** Wire a thread to its current CPU.
 *
 * Increases the wire count of a thread to ensure that it will not be
 * migrated to another CPU.
 *
 * @param thread	Thread to wire.
 */
void thread_wire(thread_t *thread) {
	if(thread) {
		spinlock_lock(&thread->lock, 0);
		thread->wire_count++;
		spinlock_unlock(&thread->lock);
	}
}

/** Unwire a thread.
 *
 * Decreases the wire count of a thread.
 *
 * @param thread	Thread to unwire.
 */
void thread_unwire(thread_t *thread) {
	if(thread) {
		spinlock_lock(&thread->lock, 0);
		if(thread->wire_count == 0) {
			fatal("Calling unwire when thread already unwired");
		}
		thread->wire_count--;
		spinlock_unlock(&thread->lock);
	}
}

/** Interrupt a sleeping thread.
 *
 * Interrupts a thread that is sleeping on a wait queue if possible. Should
 * only be called by wait queue code, as it does not take the wait queue lock.
 *
 * @param thread	Thread to interrupt.
 *
 * @return		Whether the thread was interrupted.
 */
bool thread_interrupt(thread_t *thread) {
	bool ret;

	spinlock_lock(&thread->lock, 0);

	assert(thread->state == THREAD_SLEEPING);
	assert(thread->waitq);

	if((ret = thread->interruptible)) {
		/* Restore the interruption context and queue it to run. */
		thread->context = thread->sleep_context;
		waitq_do_wake(thread);
	}

	spinlock_unlock(&thread->lock);
	return ret;
}

/** Handler for a thread kill IPI.
 * @note		Does not actually do anything, simply serves as an
 *			interruption in case the thread is in userspace, so
 *			that it gets killed when returning from the interrupt.
 * @param msg		IPI message structure.
 * @param data1		Unused.
 * @param data2		Unused.
 * @param data3		Unused.
 * @param data4		Unused.
 * @return		Always returns 0. */
static int thread_kill_handler(void *msg, unative_t data1, unative_t data2,
                               unative_t data3, unative_t data4) {
	return 0;
}

/** Request that a thread terminates.
 *
 * Ask a userspace thread to terminate as soon as possible (upon next exit from
 * the kernel). If the thread is currently in interruptible sleep, it will be
 * interrupted. You cannot make a kernel thread terminate.
 *
 * @param thread	Thread to kill.
 */
void thread_kill(thread_t *thread) {
	spinlock_lock(&thread->lock, 0);

	thread->killed = true;
	if(thread->state == THREAD_SLEEPING && thread->interruptible) {
		thread->context = thread->sleep_context;
		waitq_do_wake(thread);
	}

	/* If the thread is on a different CPU, send the CPU an IPI. */
	if(thread->state == THREAD_RUNNING && thread->cpu != curr_cpu) {
		ipi_send(thread->cpu->id, thread_kill_handler, 0, 0, 0, 0, 0);
	}

	spinlock_unlock(&thread->lock);
}

/** Terminate the current thread.
 * @note		Does not return. */
void thread_exit(void) {
	curr_thread->state = THREAD_DEAD;
	sched_yield();
	fatal("Shouldn't get here");
}

/** Rename a thread.
 *
 * Changes the name of a thread.
 *
 * @param thread	Thread to rename.
 * @param name		New name for the thread.
 */
void thread_rename(thread_t *thread, const char *name) {
	spinlock_lock(&thread->lock, 0);
	strncpy(thread->name, name, THREAD_NAME_MAX);
	thread->name[THREAD_NAME_MAX - 1] = 0;
	spinlock_unlock(&thread->lock);
}

/** Lookup a thread.
 *
 * Looks for a thread with the specified id in the thread tree. Created/dead
 * threads cannot be looked up.
 *
 * @param id		ID of the thread to find.
 *
 * @return		Pointer to thread found, or NULL if not found.
 */
thread_t *thread_lookup(identifier_t id) {
	thread_t *thread;

	if(atomic_get(&kdbg_running)) {
		thread = avl_tree_lookup(&thread_tree, (key_t)id);
	} else {
		mutex_lock(&thread_tree_lock, 0);
		thread = avl_tree_lookup(&thread_tree, (key_t)id);
		mutex_unlock(&thread_tree_lock);
	}

	if(thread && (thread->state == THREAD_DEAD || thread->state == THREAD_CREATED)) {
		thread = NULL;
	}
	return thread;
}

/** Create a new thread.
 *
 * Creates a new thread that will begin execution at the given function and
 * places it in the Created state.
 *
 * @todo		If the thread is not tied to the current CPU, pick the
 *			best CPU for it to run on.
 *
 * @param name		Name to give the thread.
 * @param owner		Process that the thread should belong to.
 * @param flags		Flags for the thread.
 * @param entry		Entry function for the thread.
 * @param arg1		First argument to pass to entry function.
 * @param arg2		Second argument to pass to entry function.
 * @param threadp	Where to store pointer to thread structure.
 *
 * @return		0 on success, a negative error code on failure.
 */
int thread_create(const char *name, process_t *owner, int flags, thread_func_t entry,
                  void *arg1, void *arg2, thread_t **threadp) {
	thread_t *thread;
	int ret;

	if(name == NULL || owner == NULL || threadp == NULL) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate a thread structure from the cache. The thread constructor
	 * caches a kernel stack with the thread for us. */
	thread = slab_cache_alloc(thread_cache, MM_SLEEP);

	strncpy(thread->name, name, THREAD_NAME_MAX);
	thread->name[THREAD_NAME_MAX - 1] = 0;

	/* Allocate a kernel stack and initialise the thread context. */
	thread->kstack = kheap_alloc(KSTACK_SIZE, MM_SLEEP);
	context_init(&thread->context, (ptr_t)thread_trampoline, thread->kstack);

	/* Initialise architecture-specific data. */
	ret = thread_arch_init(thread);
	if(ret != 0) {
		kheap_free(thread->kstack, KSTACK_SIZE);
		slab_cache_free(thread_cache, thread);
		return ret;
	}

	/* Allocate an ID for the thread. */
	thread->id = (identifier_t)vmem_alloc(thread_id_arena, 1, MM_SLEEP);

	/* Initially set the CPU to NULL - the thread will be assigned to a
	 * CPU when thread_run() is called on it. */
	thread->cpu = NULL;

	atomic_set(&thread->in_usermem, 0);
	refcount_set(&thread->count, 1);
	thread->fpu = NULL;
	thread->wire_count = 0;
	thread->killed = false;
	thread->flags = flags;
	thread->priority = 0;
	thread->timeslice = 0;
	thread->preempt_off = 0;
	thread->preempt_missed = false;
	thread->waitq = NULL;
	thread->interruptible = false;
	thread->state = THREAD_CREATED;
	thread->entry = entry;
	thread->arg1 = arg1;
	thread->arg2 = arg2;

	/* Add the thread to the owner. */
	process_attach(owner, thread);

	/* Add to the thread tree. */
	mutex_lock(&thread_tree_lock, 0);
	avl_tree_insert(&thread_tree, (key_t)thread->id, thread, NULL);
	mutex_unlock(&thread_tree_lock);

	*threadp = thread;

	dprintf("thread: created thread %" PRId32 "(%s) (thread: %p, owner: %p)\n",
		thread->id, thread->name, thread, owner);
	return 0;
}

/** Destroy a thread.
 *
 * Decreases the reference count of a thread, and queues it for deletion if it
 * reaches 0.
 *
 * @note		Because avl_tree_remove() uses kfree(), we cannot
 *			remove the thread from the thread tree here as it can
 *			be called by the scheduler. To prevent the thread from
 *			being searched for we check the thread state in
 *			thread_lookup(), and return NULL if the thread found is
 *			not running.
 *
 * @param thread	Thread to destroy.
 */
void thread_destroy(thread_t *thread) {
	spinlock_lock(&thread->lock, 0);

	if(refcount_dec(&thread->count) > 0) {
		spinlock_unlock(&thread->lock);
		return;
	}

	dprintf("thread: queueing thread %" PRId32 "(%s) for deletion (owner: %" PRId32 ")\n",
		thread->id, thread->name, thread->owner->id);

	assert(list_empty(&thread->header));
	assert(thread->state == THREAD_CREATED || thread->state == THREAD_DEAD);

	/* Queue for deletion by the thread reaper. */
	spinlock_lock(&dead_thread_lock, 0);
	list_append(&dead_threads, &thread->header);
	semaphore_up(&dead_thread_sem, 1);
	spinlock_unlock(&dead_thread_lock);

	spinlock_unlock(&thread->lock);
}

/** Print information about a thread.
 * @param thread	Thread to print.
 * @param level		Log level. */
static inline void thread_dump(thread_t *thread, int level) {
	kprintf(level, "%-5" PRId32 "%s ", thread->id,
	        (thread == curr_thread) ? "*" : " ");

	switch(thread->state) {
	case THREAD_CREATED:	kprintf(level, "Created  "); break;
	case THREAD_READY:	kprintf(level, "Ready    "); break;
	case THREAD_RUNNING:	kprintf(level, "Running  "); break;
	case THREAD_SLEEPING:	kprintf(level, "Sleeping "); break;
	case THREAD_DEAD:	kprintf(level, "Dead     "); break;
	default:		kprintf(level, "Bad      "); break;
	}

	kprintf(level, "%-4" PRIu32 " %-4d %-4zu %-5d %-20s %-5" PRId32 " %s\n",
	        (thread->cpu) ? thread->cpu->id : 0, thread->wire_count, thread->priority,
	        thread->flags, (thread->waitq) ? thread->waitq->name : "None",
	        thread->owner->id, thread->name);
}

/** Kill a thread.
 *
 * Kills a thread.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_kill(int argc, char **argv) {
	thread_t *thread;
	unative_t tid;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<thread ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Kills a currently running thread. The thread will die after KDBG exits.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &tid, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	} else if(!(thread = thread_lookup(tid))) {
		kprintf(LOG_NONE, "Invalid thread ID.\n");
		return KDBG_FAIL;
	}

	thread_kill(thread);
	return KDBG_OK;
}

/** Dump a list of threads.
 *
 * Dumps out a list of threads.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_thread(int argc, char **argv) {
	process_t *process;
	thread_t *thread;
	unative_t pid;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<process ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all threads, or a list of threads within a process\n");
		kprintf(LOG_NONE, "if given a process ID. The ID is given as an expression.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID     State    CPU  Wire Prio Flags WaitQ                Owner Name\n");
	kprintf(LOG_NONE, "==     =====    ===  ==== ==== ===== =====                ===== ====\n");

	if(argc == 2) {
		/* Find the process ID. */
		if(kdbg_parse_expression(argv[1], &pid, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(process = process_lookup(pid))) {
			kprintf(LOG_NONE, "Invalid process ID.\n");
			return KDBG_FAIL;
		}

		LIST_FOREACH(&process->threads, iter) {
			thread = list_entry(iter, thread_t, owner_link);
			thread_dump(thread, LOG_NONE);
		}
	} else {
		AVL_TREE_FOREACH(&thread_tree, iter) {
			thread = avl_tree_entry(iter, thread_t);
			thread_dump(thread, LOG_NONE);
		}
	}

	return KDBG_OK;
}

/** Initialise the thread cache. */
void __init_text thread_init(void) {
	thread_id_arena = vmem_create("thread_id_arena", 1, 65535, 1, NULL, NULL, NULL, 0, 0, MM_FATAL);
	thread_cache = slab_cache_create("thread_cache", sizeof(thread_t), 0,
	                                 thread_cache_ctor, NULL, NULL, NULL,
	                                 SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}

/** Create the thread reaper. */
void __init_text thread_reaper_init(void) {
	thread_t *thread;

	if(thread_create("reaper", kernel_proc, 0, thread_reaper, NULL, NULL, &thread) != 0) {
		fatal("Could not create thread reaper");
	}
	thread_run(thread);
}

#if 0
# pragma mark Process handle functions.
#endif

/** Closes a handle to a thread.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int thread_handle_close(handle_info_t *info) {
	thread_destroy(info->data);
	return 0;
}

/** Thread handle operations. */
static handle_type_t thread_handle_type = {
	.id = HANDLE_TYPE_THREAD,
	.close = thread_handle_close,
};

#if 0
# pragma mark System calls.
#endif

/** Thread creation arguments structure. */
typedef struct sys_thread_args {
	ptr_t sp;			/**< Stack pointer. */
	ptr_t entry;			/**< Entry point address. */
	ptr_t arg;			/**< Argument. */
} sys_thread_args_t;

/** Entry function for a userspace thread.
 * @param _args		Argument structure address.
 * @param arg2		Unused. */
static void sys_thread_create_entry(void *_args, void *arg2) {
	sys_thread_args_t *args = _args;
	ptr_t entry, sp, arg;

	entry = args->entry;
	sp = args->sp;
	arg = args->arg;
	kfree(args);

	thread_arch_enter_userspace(entry, sp, arg);
}

/** Create a new thread.
 *
 * Creates a new userspace thread under the calling process.
 *
 * @param name		Name of the thread to create.
 * @param stack		Pointer to base of stack to use for thread. If NULL,
 *			then a new stack will be allocated.
 * @param stacksz	Size of stack. If a stack is provided, then this should
 *			be the size of that stack. Otherwise, it is used as the
 *			size of the stack to create - if it is zero then a
 *			stack of the default size will be allocated.
 * @param func		Function to execute.
 * @param arg		Argument to pass to thread.
 */
handle_t sys_thread_create(const char *name, void *stack, size_t stacksz, void (*func)(void *), void *arg) {
	sys_thread_args_t *args;
	thread_t *thread = NULL;
	handle_t handle = -1;
	char *kname;
	int ret;

	if(stack && (stacksz < STACK_DELTA)) {
		return -ERR_PARAM_INVAL;
	} else if((ret = strndup_from_user(name, THREAD_NAME_MAX, MM_SLEEP, &kname)) != 0) {
		return ret;
	}

	/* Create arguments structure. */
	args = kmalloc(sizeof(sys_thread_args_t), MM_SLEEP);
	args->entry = (ptr_t)func;
	args->arg = (ptr_t)arg;

	/* Create the thread, but do not run it yet. */
	if((ret = thread_create(kname, curr_proc, 0, sys_thread_create_entry, args, NULL, &thread)) != 0) {
		goto fail;
	}

	/* Try to create the handle for the thread. */
	if((handle = handle_create(&curr_proc->handles, &thread_handle_type, thread)) < 0) {
		ret = (int)handle;
		goto fail;
	}
	refcount_inc(&thread->count);

	/* Create a userspace stack. TODO: Stack direction! */
	if(stack) {
		args->sp = (ptr_t)stack + (stacksz - STACK_DELTA);
	} else {
		if(stacksz) {
			stacksz = ROUND_UP(stacksz, PAGE_SIZE);
		} else {
			stacksz = USTACK_SIZE;
		}

		if((ret = vm_map_anon(curr_proc->aspace, 0, stacksz, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, &args->sp)) != 0) {
			goto fail;
		}
		args->sp += (stacksz - STACK_DELTA);
	}

	kfree(kname);
	thread_run(thread);
	return handle;
fail:
	if(handle >= 0) {
		handle_close(&curr_proc->handles, handle);
	}
	if(thread) {
		thread_destroy(thread);
	}
	kfree(args);
	kfree(kname);
	return (handle_t)ret;
}

/** Open a handle to a thread.
 *
 * Opens a handle to a thread in order to perform other operations on it.
 *
 * @param id		Global ID of the thread to open.
 */
handle_t sys_thread_open(identifier_t id) {
	thread_t *thread;
	handle_t handle;

	if(!(thread = thread_lookup(id))) {
		return -ERR_NOT_FOUND;
	}

	refcount_inc(&thread->count);

	if((handle = handle_create(&curr_proc->handles, &thread_handle_type, thread)) < 0) {
		thread_destroy(thread);
	}

	return handle;
}

/** Get the ID of a thread.
 *
 * Gets the ID of the thread referred to by a handle. If the handle is
 * specified as -1, then the ID of the calling thread will be returned.
 *
 * @param handle	Handle for thread to get ID of.
 *
 * @return		Thread ID on success (greater than or equal to zero),
 *			negative error code on failure.
 */
identifier_t sys_thread_id(handle_t handle) {
	handle_info_t *info;
	thread_t *thread;
	identifier_t id;

	if(handle == -1) {
		id = curr_thread->id;
	} else if(!(id = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_THREAD, &info))) {
		thread = info->data;
		id = thread->id;
		handle_release(info);
	}

	return id;
}

/** Terminate the calling thread.
 *
 * Terminates execution of the calling thread.
 *
 * @todo		Status.
 *
 * @param status	Exit status code.
 */
void sys_thread_exit(int status) {
	thread_exit();
}