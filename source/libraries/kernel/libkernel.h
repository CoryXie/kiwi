/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Kernel library header.
 */

#ifndef __LIBKERNEL_H
#define __LIBKERNEL_H

#include <kernel/process.h>
#include <kernel/status.h>

#include <util/list.h>

#include <elf.h>
#include <stdio.h>

/** Compiler attribute/builtin macros. */
#define __export		__attribute__((visibility("default")))
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

/** Round a value up. */
#define ROUND_UP(value, nearest) \
	__extension__ \
	({ \
		typeof(value) __n = value; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
			__n += nearest; \
		} \
		__n; \
	})

/** Round a value down. */
#define ROUND_DOWN(value, nearest) \
	__extension__ \
	({ \
		typeof(value) __n = value; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
		} \
		__n; \
	})

/** Whether to enable debug output. */
#define LIBKERNEL_DEBUG		0

/** Size of the heap. */
#define LIBKERNEL_HEAP_SIZE	16384

#include "arch.h"

/** Structure describing a loaded image. */
typedef struct rtld_image {
	list_t header;			/**< Link to loaded images library. */

	const char *name;		/**< Shared object name of the library. */
	const char *path;		/**< Full path to image file. */
	int refcount;			/**< Reference count (tracks what is using the image). */
	elf_addr_t dynamic[ELF_DT_NUM];	/**< Dynamic section entries. */
	elf_dyn_t *dyntab;		/**< Pointer to unmodified dynamic section. */

	void *load_base;		/**< Base address for the image. */
	size_t load_size;		/**< Size of the image's memory region. */

	Elf32_Word *h_buckets;		/**< Hash table buckets. */
	int h_nbucket;			/**< Number of hash buckets. */
	Elf32_Word *h_chains;		/**< Hash table chains. */
	int h_nchain;			/**< Number of chain entries. */

	/** State of the image. */
	enum {
		RTLD_IMAGE_LOADING,	/**< Image is currently being loaded. */
		RTLD_IMAGE_LOADED,	/**< Image is fully loaded. */
	} state;
} rtld_image_t;

extern list_t loaded_images;
extern rtld_image_t libkernel_image;
extern rtld_image_t *application_image;

#if LIBKERNEL_DEBUG
# define dprintf(fmt...)	printf(fmt)
#else
# define dprintf(fmt...)	
#endif

extern status_t rtld_image_relocate(rtld_image_t *image);
extern status_t rtld_image_load(const char *path, rtld_image_t *req, int type, void **entryp, rtld_image_t **imagep);
extern void rtld_image_unload(rtld_image_t *image);
extern bool rtld_symbol_lookup(rtld_image_t *start, const char *name, elf_addr_t *addrp);
extern void rtld_symbol_init(rtld_image_t *image);
extern void *rtld_init(process_args_t *args);

extern void libkernel_arch_init(process_args_t *args, rtld_image_t *image);
extern void libkernel_heap_init(void);
extern void libkernel_init(process_args_t *args);

#endif /* __LIBKERNEL_H */
