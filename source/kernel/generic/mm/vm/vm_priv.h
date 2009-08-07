/* Kiwi private VM system definitions
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
 * @brief		Private VM system definitions.
 */

#ifndef __VM_PRIV_H
#define __VM_PRIV_H

#include <console/kprintf.h>

#include <mm/page.h>
#include <mm/vm.h>

#if CONFIG_VM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Check if a range fits in an address space. */
#if ASPACE_BASE == 0
# define vm_region_fits(start, size)	(((start) + (size)) <= ASPACE_SIZE)
#else
# define vm_region_fits(start, size)	((start) >= ASPACE_BASE && ((start) + (size)) <= (ASPACE_BASE + ASPACE_SIZE))
#endif

/** Convert region flags to page map flags.
 * @param flags         Flags to convert.
 * @return              Page map flags. */
static inline int vm_region_flags_to_page(int flags) {
        int ret = 0;

        ret |= ((flags & VM_REGION_READ) ? PAGE_MAP_READ : 0);
        ret |= ((flags & VM_REGION_WRITE) ? PAGE_MAP_WRITE : 0);
       	ret |= ((flags & VM_REGION_EXEC) ? PAGE_MAP_EXEC : 0);
        return ret;
}

/** Architecture hooks. */
extern int vm_aspace_arch_init(vm_aspace_t *as);

/** Anonymous object functions. */
extern vm_object_t *vm_anon_object_create(size_t size, vm_object_t *source, offset_t offset);
extern void vm_anon_object_destroy(vm_object_t *obj);

/** Initialization functions. */
extern void vm_anon_init(void);
extern void vm_page_init(void);

#endif /* __VM_PRIV_H */