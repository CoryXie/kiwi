/*
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
 * @brief		AMD64 paging functions.
 *
 * @todo		Free up page tables properly.
 */

#include <arch/barrier.h>
#include <arch/features.h>
#include <arch/memmap.h>
#include <arch/sysreg.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>

/** Kernel paging structures. */
extern uint64_t __boot_pml4[];
extern uint64_t __kernel_pdp[];

/** Symbols defined by the linker script. */
extern char __text_start[], __text_end[], __rodata_start[], __rodata_end[];
extern char __bss_end[], __end[];

#if 0
# pragma mark Page map functions.
#endif

/** Kernel page map. */
page_map_t kernel_page_map;

/** Convert page map flags to PTE flags.
 * @param flags		Flags to convert.
 * @return		Converted flags. */
static inline uint64_t page_map_flags_to_pte(int prot) {
	uint64_t ret = 0;

	ret |= ((prot & PAGE_MAP_WRITE) ? PG_WRITE : 0);
#if CONFIG_X86_NX
	ret |= ((!(prot & PAGE_MAP_EXEC) && CPU_HAS_XD(curr_cpu)) ? PG_NOEXEC : 0);
#endif
	return ret;
}

/** Get the page table containing an address.
 * @param map		Page map to get from.
 * @param virt		Address to get page table for.
 * @param alloc		Whether to allocate structures if not found.
 * @param mmflag	Allocation flags.
 * @param ptblp		Where to store (virtual) pointer to page table.
 * @return		0 on success, negative error code on failure. */
static int page_map_get_ptbl(page_map_t *map, ptr_t virt, bool alloc, int mmflag, uint64_t **ptblp) {
	uint64_t *pml4, *pdp, *pdir;
	int pml4e, pdpe, pde;
	phys_ptr_t page;

	/* Get the virtual address of the PML4. Note that unmapping is not
	 * necessary because of our page_phys_map() implementation. */
	pml4 = page_phys_map(map->pml4, PAGE_SIZE, mmflag);

	/* Get the page directory pointer number. A PDP covers 512GB. */
	pml4e = (virt & 0x0000FFFFFFFFF000) / 0x8000000000;
	if(!(pml4[pml4e] & PG_PRESENT)) {
		/* Allocate a new PDP if required. Safe to use PM_ZERO because
		 * our implementation of page_phys_map() doesn't touch the
		 * heap. Allocating a page can cause page mappings to be
		 * modified (if a Vmem boundary tag refill occurs), handle
		 * this possibility. */
		if(alloc) {
			page = page_alloc(1, mmflag | PM_ZERO);
			if(pml4[pml4e] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(!page) {
					return -ERR_NO_MEMORY;
				}

				/* Map it into the PML4. */
				pml4[pml4e] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
			}
		} else {
			return -ERR_NOT_FOUND;
		}
	}

	pdp = page_phys_map((phys_ptr_t)(pml4[pml4e] & PAGE_MASK), PAGE_SIZE, mmflag);

	/* Get the page directory number. A page directory covers 1GB. */
	pdpe = (virt % 0x8000000000) / 0x40000000;
	if(!(pdp[pdpe] & PG_PRESENT)) {
		/* Allocate a new page directory if required. */
		if(alloc) {
			page = page_alloc(1, mmflag | PM_ZERO);
			if(pdp[pdpe] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(!page) {
					return -ERR_NO_MEMORY;
				}

				/* Map it into the PDP. */
				pdp[pdpe] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
			}
		} else {
			return -ERR_NOT_FOUND;
		}
	}

	pdir = page_phys_map((phys_ptr_t)(pdp[pdpe] & PAGE_MASK), PAGE_SIZE, mmflag);

	/* Get the page table number. A page table covers 2MB. */
	pde = (virt % 0x40000000) / 0x200000;
	if(!(pdir[pde] & PG_PRESENT)) {
		/* Allocate a new page table if required. */
		if(alloc) {
			page = page_alloc(1, mmflag | PM_ZERO);
			if(pdir[pde] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(!page) {
					return -ERR_NO_MEMORY;
				}

				/* Map it into the page directory. */
				pdir[pde] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
			}
		} else {
			return -ERR_NOT_FOUND;
		}
	}

	assert(!(pdir[pde] & PG_LARGE));
	*ptblp = page_phys_map((phys_ptr_t)(pdir[pde] & PAGE_MASK), PAGE_SIZE, mmflag);
	return 0;
}

/** Insert a mapping in a page map.
 *
 * Maps a virtual address to a physical address with the given protection
 * settings in a page map.
 *
 * @param map		Page map to insert in.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to.
 * @param prot		Protection flags.
 * @param mmflag	Page allocation flags.
 *
 * @return		0 on success, negative error code on failure. Can only
 *			fail if MM_SLEEP is not set.
 */
int page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, int prot, int mmflag) {
	uint64_t *ptbl;
	int pte, ret;

	assert(!(virt % PAGE_SIZE));
	assert(!(phys % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Check that we can map here. */
	if(virt < map->first || virt > map->last) {
		fatal("Map on %p outside allowed area", map);
	}

	/* Find the page table for the entry. */
	if((ret = page_map_get_ptbl(map, virt, true, mmflag, &ptbl)) != 0) {
		mutex_unlock(&map->lock);
		return ret;
	}

	/* Check that the mapping doesn't already exist. */
	pte = (virt % 0x200000) / PAGE_SIZE;
	if(ptbl[pte] & PG_PRESENT) {
		fatal("Mapping %p which is already mapped", virt);
	}

	/* Map the address in. */
	ptbl[pte] = phys | PG_PRESENT | ((map->user) ? PG_USER : PG_GLOBAL) | page_map_flags_to_pte(prot);
	memory_barrier();
	mutex_unlock(&map->lock);
	return 0;
}

/** Remove a mapping from a page map.
 *
 * Removes the mapping at a virtual address from a page map.
 *
 * @param map		Page map to unmap from.
 * @param virt		Virtual address to unmap.
 * @param physp		Where to store mapping's physical address prior to
 *			unmapping (can be NULL).
 *
 * @return		0 on success, -ERR_NOT_FOUND if mapping missing.
 */
int page_map_remove(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	uint64_t *ptbl;
	int pte, ret;

	assert(!(virt % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Check that we can unmap here. */
	if(virt < map->first || virt > map->last) {
		fatal("Unmap on %p outside allowed area", map);
	}

	/* Find the page table for the entry. */
	if((ret = page_map_get_ptbl(map, virt, false, 0, &ptbl)) != 0) {
		mutex_unlock(&map->lock);
		return ret;
	}

	pte = (virt % 0x200000) / PAGE_SIZE;
	if(ptbl[pte] & PG_PRESENT) {
		/* Store the physical address if required. */
		if(physp != NULL) {
			*physp = ptbl[pte] & PAGE_MASK;
		}

		/* Clear the entry. */
		ptbl[pte] = 0;
		memory_barrier();
		mutex_unlock(&map->lock);
		return 0;
	} else {
		mutex_unlock(&map->lock);
		return -ERR_NOT_FOUND;
	}
}

/** Get the value of a mapping in a page map.
 *
 * Gets the physical address, if any, that a virtual address is mapped to in
 * a page map.
 *
 * @param map		Page map to lookup in.
 * @param virt		Address to find.
 * @param physp		Where to store mapping's value.
 *
 * @return		True if mapping is present, false if not.
 */
bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	uint64_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));
	assert(physp);

	mutex_lock(&map->lock, 0);

	/* Find the page table for the entry. */
	if(page_map_get_ptbl(map, virt, false, 0, &ptbl) == 0) {
		pte = (virt % 0x200000) / PAGE_SIZE;
		if(ptbl[pte] & PG_PRESENT) {
			*physp = ptbl[pte] & PAGE_MASK;
			mutex_unlock(&map->lock);
			return true;
		}
	}

	mutex_unlock(&map->lock);
	return false;
}

/** Modify protection flags of a range.
 *
 * Modifies the protection flags of a range of pages in a page map. If any of
 * the pages in the range are not mapped, then the function will ignore it and
 * move on to the next.
 *
 * @param map		Map to modify in.
 * @param start		Start of page range.
 * @param end		End of page range.
 * @param prot		New protection flags.
 *
 * @return		0 on success, negative error code on failure.
 */
int page_map_protect(page_map_t *map, ptr_t start, ptr_t end, int prot) {
	uint64_t *ptbl;
	ptr_t i;
	int pte;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	for(i = start; i < end; i++) {
		pte = (i % 0x200000) / PAGE_SIZE;

		if(page_map_get_ptbl(map, i, false, 0, &ptbl) != 0 || !(ptbl[pte] & PG_PRESENT)) {
			continue;
		}

		/* Clear out original flags, and set the new flags. */
		ptbl[pte] = (ptbl[pte] & ~(PG_WRITE | PG_NOEXEC)) | page_map_flags_to_pte(prot);
	}

	mutex_unlock(&map->lock);
	return 0;
}

/** Switch to a different page map.
 *
 * Switches to a different page map.
 *
 * @param map		Page map to switch to.
 */
void page_map_switch(page_map_t *map) {
	sysreg_cr3_write(map->pml4);
}

/** Initialise a page map structure.
 *
 * Initialises a userspace page map structure.
 *
 * @param map		Page map to initialise.
 *
 * @return		0 on success, negative error code on failure.
 */
int page_map_init(page_map_t *map) {
	uint64_t *pml4;

	mutex_init(&map->lock, "page_map_lock", MUTEX_RECURSIVE);
	map->pml4 = page_alloc(1, MM_SLEEP | PM_ZERO);
	map->user = true;
	map->first = USER_MEMORY_BASE;
	map->last = (USER_MEMORY_BASE + USER_MEMORY_SIZE) - PAGE_SIZE;

	/* Get the kernel mappings into the new PML4. */
	pml4 = page_phys_map(map->pml4, PAGE_SIZE, MM_SLEEP);
	pml4[511] = __boot_pml4[511] & ~PG_ACCESSED;
	return 0;
}

/** Destroy a page map.
 *
 * Destroys all mappings in a page map and frees up anything it has allocated.
 *
 * @todo		Implement this properly.
 *
 * @param map		Page map to destroy.
 */
void page_map_destroy(page_map_t *map) {
	page_free(map->pml4, 1);
}

#if 0
# pragma mark Physical memory access functions.
#endif

/** Map physical memory into the kernel address space.
 *
 * Maps a range of physical memory into the kernel's address space. The
 * range does not have to be page-aligned. When the memory is no longer needed,
 * the mapping should be removed with page_phys_unmap().
 *
 * @param addr		Base of range to map.
 * @param size		Size of range to map.
 * @param mmflag	Allocation flags.
 *
 * @return		Virtual address of mapping.
 */
void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag) {
	if(size == 0) {
		return NULL;
	}
	return (void *)((ptr_t)addr + KERNEL_PMAP_BASE);
}

/** Unmap physical memory.
 *
 * Unmaps a range of physical memory previously mapped with page_phys_map().
 *
 * @param addr		Virtual address returned from page_phys_map().
 * @param size		Size of original mapping.
 * @param shared	Whether this mapping was used by any other CPUs.
 */
void page_phys_unmap(void *addr, size_t size, bool shared) {
	/* Nothing happens. */
}

#if 0
# pragma mark Initialisation functions.
#endif

/** Convert a large page to a page table if necessary.
 * @param virt		Virtual address to check. */
static void __init_text page_large_to_ptbl(ptr_t virt) {
	uint64_t *pdir, *ptbl;
	int pdpe, pde, i;
	phys_ptr_t page;

	pdpe = (virt % 0x8000000000) / 0x40000000;
	if(!(__kernel_pdp[pdpe] & PG_PRESENT)) {
		return;
	}

	pdir = page_phys_map((phys_ptr_t)(__kernel_pdp[pdpe] & PAGE_MASK), PAGE_SIZE, MM_FATAL);

	pde = (virt % 0x40000000) / 0x200000;
	if(pdir[pde] & PG_LARGE) {
		page = page_alloc(1, MM_FATAL);
		ptbl = page_phys_map(page, PAGE_SIZE, MM_FATAL);
		memset(ptbl, 0, PAGE_SIZE);

		/* Set pages and copy all flags from the PDE. */
		for(i = 0; i < 512; i++) {
			ptbl[i] = (pdir[pde] & ~(PG_LARGE | PG_ACCESSED)) + (i * PAGE_SIZE);
		}

		/* Replace the large page in the page directory. */
		pdir[pde] = page | PG_PRESENT | PG_WRITE;
		__asm__ volatile("invlpg (%0)" :: "r"(ROUND_DOWN(virt, 0x200000)));
	}
}

#if CONFIG_X86_NX
/** Set a flag on a range of pages.
 * @param flag		Flag to set.
 * @param start		Start virtual address.
 * @param end		End virtual address.  */
static inline void page_set_flag(uint64_t flag, ptr_t start, ptr_t end) {
	uint64_t *ptbl;
	ptr_t i;
	int ret;

	assert(start >= KERNEL_VIRT_BASE);
	assert((start % PAGE_SIZE) == 0);
	assert((end % PAGE_SIZE) == 0);

	for(i = start; i < end; i += PAGE_SIZE) {
		page_large_to_ptbl(i);

		if((ret = page_map_get_ptbl(&kernel_page_map, i, false, 0, &ptbl)) != 0) {
			fatal("Could not get kernel page table (%d)", ret);
		}

		ptbl[(i % 0x200000) / PAGE_SIZE] |= flag;
		__asm__ volatile("invlpg (%0)" :: "r"(i));
	}
}
#endif

/** Set a flag on a range of pages.
 * @param flag		Flag to set.
 * @param start		Start virtual address.
 * @param end		End virtual address.  */
static void __init_text page_clear_flag(uint64_t flag, ptr_t start, ptr_t end) {
	uint64_t *ptbl;
	ptr_t i;
	int ret;

	assert(start >= KERNEL_VIRT_BASE);
	assert((start % PAGE_SIZE) == 0);
	assert((end % PAGE_SIZE) == 0);

	for(i = start; i < end; i += PAGE_SIZE) {
		page_large_to_ptbl(i);

		if((ret = page_map_get_ptbl(&kernel_page_map, i, false, 0, &ptbl)) != 0) {
			fatal("Could not get kernel page table (%d)", ret);
		}

		ptbl[(i % 0x200000) / PAGE_SIZE] &= ~flag;
		__asm__ volatile("invlpg (%0)" :: "r"(i));
	}
}

/** Set up the kernel page map. */
void __init_text page_arch_init(void) {
	mutex_init(&kernel_page_map.lock, "kernel_page_map_lock", MUTEX_RECURSIVE);
	kernel_page_map.pml4 = KA2PA(__boot_pml4);
	kernel_page_map.user = false;
	kernel_page_map.first = KERNEL_HEAP_BASE;
	kernel_page_map.last = (ptr_t)-PAGE_SIZE;

	kprintf(LOG_DEBUG, "page: initialised kernel page map (pml4: 0x%" PRIpp ")\n", kernel_page_map.pml4);
#if CONFIG_X86_NX
	/* Enable NX/XD if supported. */
	if(CPU_HAS_XD(curr_cpu)) {
		kprintf(LOG_DEBUG, "page: CPU supports NX/XD, enabling...\n");
		sysreg_msr_write(SYSREG_MSR_EFER, sysreg_msr_read(SYSREG_MSR_EFER) | SYSREG_EFER_NXE);
	}
#endif
}

/** Mark kernel sections as read-only/no-execute and unmap identity mapping. */
void __init_text page_late_init(void) {
	/* Mark .text and .rodata as read-only. OK to round down - __text_start
	 * is only non-aligned because of the SIZEOF_HEADERS in the linker
	 * script. */
	page_clear_flag(PG_WRITE, ROUND_DOWN((ptr_t)__text_start, PAGE_SIZE), (ptr_t)__text_end);
	page_clear_flag(PG_WRITE, (ptr_t)__rodata_start, (ptr_t)__rodata_end);
	kprintf(LOG_DEBUG, "page: marked sections (.text .rodata) as read-only\n");

#if CONFIG_X86_NX
	/* Mark sections of the kernel no-execute if supported. */
	if(CPU_HAS_XD(curr_cpu)) {
		/* Assumes certain layout in linker script: .rodata, .data and
		 * then .bss. */
		page_set_flag(PG_NOEXEC, (ptr_t)__rodata_start, (ptr_t)__bss_end);
		kprintf(LOG_DEBUG, "page: marked sections (.rodata .data .bss) as no-execute\n");
	}
#endif

	/* Clear identity mapping. */
	__boot_pml4[0] = 0;
	memory_barrier();

	/* Force a complete TLB wipe - the global flag is set on pages on the
	 * identity mapping because we use the kernel PDP for it. */
	sysreg_cr4_write(sysreg_cr4_read() & ~SYSREG_CR4_PGE);
	sysreg_cr4_write(sysreg_cr4_read() | SYSREG_CR4_PGE);
}
