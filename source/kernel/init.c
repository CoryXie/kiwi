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
 * @brief		Kernel initialisation functions.
 */

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <io/device.h>
#include <io/fs.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/session.h>
#include <proc/thread.h>

#include <security/context.h>

#include <console.h>
#include <fatal.h>
#include <init.h>
#include <kargs.h>
#include <lrm.h>
#include <module.h>
#include <object.h>
#include <status.h>
#include <symbol.h>
#include <tar.h>
#include <time.h>
#include <version.h>

/** Structure describing a boot module. */
typedef struct boot_module {
	list_t header;			/**< Link to modules list. */
	void *mapping;			/**< Pointer to mapped module data. */
	size_t size;			/**< Size of the module data. */
	object_handle_t *handle;	/**< File handle for the module data. */
	char *name;			/**< Name of the module. */
} boot_module_t;

extern void kmain(kernel_args_t *args, uint32_t cpu);
extern initcall_t __initcall_start[], __initcall_end[];
extern fs_mount_t *root_mount;

/** Rendezvous variables for SMP boot. */
static atomic_t init_rendezvous_1 __init_data = 0;
static atomic_t init_rendezvous_2 __init_data = 0;
static atomic_t init_rendezvous_3 __init_data = 0;

/** The amount to increment the boot progress for each module. */
static int current_init_progress __init_data = 10;
static int progress_per_module __init_data;

/** Lock to serialise the SMP boot. */
static SPINLOCK_DECLARE(smp_boot_spinlock);

/** List of modules/FS images from the bootloader. */
static LIST_DECLARE(boot_module_list);
static LIST_DECLARE(boot_fsimage_list);

/** Wait until all CPUs reach a certain point.
 * @param args		Kernel arguments structure.
 * @param var		Variable to wait on. */
static inline void init_rendezvous(kernel_args_t *args, atomic_t *var) {
	atomic_inc(var);
	while(atomic_get(var) < (int)args->cpu_count);
}

/** Remove a module from the module list.
 * @param mod		Module to remove. */
static void __init_text boot_module_remove(boot_module_t *mod) {
	list_remove(&mod->header);
	if(mod->name) {
		kfree(mod->name);
	}
	object_handle_release(mod->handle);
	page_phys_unmap(mod->mapping, mod->size, true);
	kfree(mod);

	/* Update progress. */
	current_init_progress += progress_per_module;
	console_update_boot_progress(current_init_progress);
}

/** Look up a kernel module in the boot module list.
 * @param name		Name to look for.
 * @return		Pointer to module if found, NULL if not. */
static boot_module_t *boot_module_lookup(const char *name) {
	boot_module_t *mod;

	LIST_FOREACH(&boot_module_list, iter) {
		mod = list_entry(iter, boot_module_t, header);

		if(mod->name && strcmp(mod->name, name) == 0) {
			return mod;
		}
	}

	return NULL;
}

/** Extract a TAR archive to the root FS.
 * @param mod		Boot module containing archive. */
static void __init_text load_boot_fsimage(boot_module_t *mod) {
	tar_header_t *hdr = mod->mapping;
	object_handle_t *handle;
	int64_t size;
	status_t ret;
	size_t bytes;

	/* Check format of module. */
	if(strncmp(hdr->magic, "ustar", 5) != 0) {
		fatal("Unknown boot module format");
	}

	/* If any TAR files are loaded it means we should mount a RamFS at the
	 * root, if this has not already been done. */
	if(!root_mount) {
		ret = fs_mount(NULL, "/", "ramfs", NULL);
		if(ret != STATUS_SUCCESS) {
			fatal("Could not mount RamFS at root (%d)", ret);
		}
	}

	/* Loop until we encounter two null bytes (EOF). */
	while(hdr->name[0] && hdr->name[1]) {
		if(strncmp(hdr->magic, "ustar", 5) != 0) {
			fatal("TAR file format is not correct");
		}

		/* All fields in the header are stored as ASCII - convert the
		 * size to an integer (base 8). */
		size = strtoll(hdr->size, NULL, 8);

		/* Handle the entry based on its type flag. */
		switch(hdr->typeflag) {
		case REGTYPE:
		case AREGTYPE:
			ret = fs_file_create(hdr->name);
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to create regular file %s (%d)", hdr->name, ret);
			}

			ret = fs_file_open(hdr->name, FS_WRITE, 0, &handle);
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to open file %s (%d)", hdr->name, ret);
			}

			ret = fs_file_write(handle, (void *)((ptr_t)hdr + 512), size, &bytes);
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to write file %s (%d)", hdr->name, ret);
			} else if((int64_t)bytes != size) {
				fatal("Did not write all data for file %s (%zu, %zu)", hdr->name, bytes, size);
			}

			object_handle_release(handle);
			break;
		case DIRTYPE:
			ret = fs_dir_create(hdr->name);
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to create directory %s (%d)", hdr->name, ret);
			}
			break;
		case SYMTYPE:
			ret = fs_symlink_create(hdr->name, hdr->linkname);
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to create symbolic link %s (%d)", hdr->name, ret);
			}
			break;
		default:
			kprintf(LOG_DEBUG, "init: unhandled TAR type flag '%c'\n", hdr->typeflag);
			break;
		}

		/* 512 for the header, plus the file size if necessary. */
		hdr = (tar_header_t *)(ptr_t)((ptr_t)hdr + 512 + ((size != 0) ? ROUND_UP(size, 512) : 0));
	}

	boot_module_remove(mod);
}

/** Load a kernel module provided at boot.
 * @param mod		Module to load. */
static void __init_text load_boot_kmod(boot_module_t *mod) {
	char name[MODULE_NAME_MAX + 1];
	boot_module_t *dep;
	status_t ret;

	/* Try to load the module and all dependencies. */
	while(true) {
		ret = module_load(mod->handle, name);
		if(ret == STATUS_SUCCESS) {
			boot_module_remove(mod);
			return;
		} else if(ret != STATUS_MISSING_LIBRARY) {
			fatal("Could not load module %s (%d)", (mod->name) ? mod->name : "<noname>", ret);
		}

		/* Unloaded dependency, try to find it and load it. */
		if(!(dep = boot_module_lookup(name))) {
			fatal("Dependency on '%s' which is not available", name);
		}
		load_boot_kmod(dep);
	}
}

/** Load modules loaded by the bootloader.
 * @param args		Kernel arguments structure. */
static void __init_text load_modules(kernel_args_t *args) {
	kernel_args_module_t *amod;
	boot_module_t *mod;
	phys_ptr_t addr;
	char *tmp;

	if(!args->module_count) {
		fatal("No modules were provided, cannot do anything!");
	}

	/* Firstly, populate our module list with the module details from the
	 * bootloader. This is done so that it's much easier to look up module
	 * dependencies, and also because the module loader requires handles
	 * rather than a chunk of memory. */
	for(addr = args->modules; addr;) {
		amod = page_phys_map(addr, sizeof(kernel_args_module_t), MM_FATAL);

		/* Create a structure for the module and map the data into
		 * memory. */
		mod = kmalloc(sizeof(boot_module_t), MM_FATAL);
		list_init(&mod->header);
		mod->name = NULL;
		mod->size = amod->size;
		mod->mapping = page_phys_map(amod->base, mod->size, MM_FATAL);
		mod->handle = fs_file_from_memory(mod->mapping, mod->size);

		/* Figure out the module name, which is needed to resolve
		 * dependencies. If unable to get the name, assume the module
		 * is a filesystem image. */
		tmp = kmalloc(MODULE_NAME_MAX + 1, MM_FATAL);
		if(module_name(mod->handle, tmp) != STATUS_SUCCESS) {
			kfree(tmp);
			list_append(&boot_fsimage_list, &mod->header);
		} else {
			mod->name = tmp;
			list_append(&boot_module_list, &mod->header);
		}

		addr = amod->next;
		page_phys_unmap(amod, sizeof(kernel_args_module_t), true);
	}

	/* Determine how much to increase the boot progress by for each
	 * module loaded. */
	progress_per_module = 80 / args->module_count;

	/* Load all kernel modules. */
	while(!list_empty(&boot_module_list)) {
		mod = list_entry(boot_module_list.next, boot_module_t, header);
		load_boot_kmod(mod);
	}
}

/** Second-stage intialization thread.
 * @param args		Kernel arguments structure pointer.
 * @param arg2		Thread argument (unused). */
static void init_thread(void *args, void *arg2) {
	//const char *pargs[] = { "/system/services/svcmgr", NULL }, *penv[] = { NULL };
	const char *pargs[] = { "/system/binaries/test", NULL }, *penv[] = { NULL };
	initcall_t *initcall;
	boot_module_t *mod;
	status_t ret;

	/* Bring up the filesystem manager and device manager. */
	device_init();
	fs_init(args);

	/* Bring up secondary CPUs. The first rendezvous sets off their
	 * initialisation, the second waits for them to complete. */
	init_rendezvous(args, &init_rendezvous_2);
	init_rendezvous(args, &init_rendezvous_3);

	/* Call other initialisation functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++) {
		(*initcall)();
	}

	console_update_boot_progress(10);

	/* Load modules, then any FS images supplied. Wait until after loading
	 * kernel modules to do FS images, so that we only load FS images if the
	 * boot filesystem could not be mounted. */
	load_modules(args);
	if(!root_mount) {
		if(list_empty(&boot_fsimage_list)) {
			fatal("Could not find boot filesystem");
		}

		while(!list_empty(&boot_fsimage_list)) {
			mod = list_entry(boot_fsimage_list.next, boot_module_t, header);
			load_boot_fsimage(mod);
		}
	} else {
		while(!list_empty(&boot_fsimage_list)) {
			mod = list_entry(boot_fsimage_list.next, boot_module_t, header);
			boot_module_remove(mod);
		}
	}

	/* Reclaim memory taken up by initialisation code/data. */
	page_late_init();

	console_update_boot_progress(100);

	/* Run the service manager. */
	ret = process_create(pargs, penv, PROCESS_CRITICAL, PRIORITY_SYSTEM, kernel_proc, NULL);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not start service manager (%d)", ret);
	}
}

/** Main function of the kernel.
 * @param args          Arguments from the bootloader.
 * @param cpu           CPU that the function is running on. */
void __init_text kmain(kernel_args_t *args, uint32_t cpu) {
	thread_t *thread;
	status_t ret;

	/* Wait for all CPUs to enter the kernel. */
	init_rendezvous(args, &init_rendezvous_1);

	if(cpu == args->boot_cpu) {
		cpu_early_init(args);
		console_early_init();

		/* Perform early architecture/platform initialisation. */
		arch_premm_init(args);
		platform_premm_init(args);

		/* Initialise kernel memory management subsystems. */
		vmem_early_init();
		kheap_early_init();
		page_init(args);
		vmem_init();
		slab_init();
		kheap_init();
		malloc_init();

		/* Set up the console. */
		console_init(args);
		kprintf(LOG_NORMAL, "kernel: version %s booting (%" PRIu32 " CPU(s))\n",
		        kiwi_ver_string, cpu_count);

		/* Perform second stage architecture/platform initialisation. */
		arch_postmm_init(args);
		platform_postmm_init(args);

		/* Bring up symbol manager and CPU-related stuff. */
		symbol_init();
		time_init();
		cpu_init(args);
		ipi_init();
#if CONFIG_DEBUGGER_DELAY > 0
		/* Delay to allow GDB to be connected. */
		kprintf(LOG_NORMAL, "kernel: waiting %d seconds for a debugger...\n", CONFIG_DEBUGGER_DELAY);
		spin(SECS2USECS(CONFIG_DEBUGGER_DELAY));
#endif
		/* Bring up process-/thread-related stuff. */
		security_init();
		handle_init();
		session_init();
		process_init();
		thread_init();
		sched_init();
		thread_reaper_init();
		lrm_init();

		/* Now that the thread system is up and all CPUs have been
		 * registered, the slab allocator's magazine layer can be
		 * enabled. */
		slab_late_init();

		/* Bring up the VM system. */
		vm_init();

		/* Create the second stage initialisation thread. */
		ret = thread_create("init", kernel_proc, 0, init_thread, args, NULL, NULL, &thread);
		if(ret != STATUS_SUCCESS) {
			fatal("Could not create second-stage initialisation thread");
		}
		thread_run(thread);

		/* Finally begin executing other threads. */
		sched_enter();
	} else {
		/* Wait for the boot CPU to do its initialisation. */
		init_rendezvous(args, &init_rendezvous_2);

		spinlock_lock(&smp_boot_spinlock);

		/* Switch to the kernel page map, set the CPU pointer and do
		 * architecture/platform initialisation. */
		page_map_switch(&kernel_page_map);
		cpu_set_pointer((ptr_t)cpus[cpu]);
		arch_ap_init(args);
		platform_ap_init(args);

		/* We're running, add ourselves to the running CPU list. */
		list_append(&cpus_running, &curr_cpu->header);

		/* Do scheduler initialisation. */
		sched_init();

		spinlock_unlock(&smp_boot_spinlock);

		/* Perform the final rendezvous and then enter the scheduler. */
		init_rendezvous(args, &init_rendezvous_3);
		sched_enter();
	}
}
