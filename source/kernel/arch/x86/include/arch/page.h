/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 paging definitions.
 */

#ifndef __ARCH_PAGE_H
#define __ARCH_PAGE_H

/** Page size definitions. */
#define PAGE_WIDTH		12		/**< Width of a page in bits. */
#define PAGE_SIZE		0x1000		/**< Size of a page (4KB). */
#define LARGE_PAGE_WIDTH	21		/**< Width of a large page in bits. */
#define LARGE_PAGE_SIZE		0x200000	/**< Size of a large page (2MB). */

/** Mask to clear page offset from a virtual address. */
#ifdef __x86_64__
# define PAGE_MASK		0x000FFFFFFFFFF000
#else
# define PAGE_MASK		0xFFFFF000
#endif

/** Definitions of paging structure bits. */
#define PG_PRESENT		(1<<0)		/**< Page is present. */
#define PG_WRITE		(1<<1)		/**< Page is writable. */
#define PG_USER			(1<<2)		/**< Page is accessible in CPL3. */
#define PG_PWT			(1<<3)		/**< Page has write-through caching. */
#define PG_NOCACHE		(1<<4)		/**< Page has caching disabled. */
#define PG_ACCESSED		(1<<5)		/**< Page has been accessed. */
#define PG_DIRTY		(1<<6)		/**< Page has been written to. */
#define PG_LARGE		(1<<7)		/**< Page is a large page. */
#define PG_GLOBAL		(1<<8)		/**< Page won't be cleared in TLB. */
#ifndef __ASM__
# define PG_NOEXEC		(1LL<<63)	/**< Page is not executable (requires NX support). */
#else
# define PG_NOEXEC		(1<<63)		/**< Page is not executable (requires NX support). */
#endif

#if !defined(__ASM__) && !defined(LOADER)

#include <sync/mutex.h>

/** Architecture-specific page map structure. */
typedef struct page_map {
	mutex_t lock;			/**< Lock to protect page map. */
#ifdef __x86_64__
	phys_ptr_t pml4;		/**< Physical address of PML4. */
#else
	phys_ptr_t pdp;			/**< Physical address of PDP. */
#endif
	bool user;			/**< Whether pages mapped should be userspace accessible. */

	/** Range covered by page map. */
	ptr_t first;			/**< First allowed page. */
	ptr_t last;			/**< Last allowed page. */
} page_map_t;

extern void page_late_init(void);

#endif /* __ASM__/LOADER */
#endif /* __ARCH_PAGE_H */