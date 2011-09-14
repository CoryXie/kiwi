/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		Kernel heap allocation functions.
 */

#ifndef __MM_HEAP_H
#define __MM_HEAP_H

#include <arch/page.h>

#include <mm/flags.h>

#include <types.h>

extern ptr_t heap_raw_alloc(size_t size, int mmflag);
extern void heap_raw_free(ptr_t addr, size_t size);

extern void *heap_alloc(size_t size, int mmflag);
extern void heap_free(void *addr, size_t size);

extern void *heap_map_range(phys_ptr_t base, size_t size, int mmflag);
extern void heap_unmap_range(void *addr, size_t size, bool shared);

extern void heap_init();

#endif /* __MM_HEAP_H */
