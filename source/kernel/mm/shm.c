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
 * @brief		Shared memory functions.
 *
 * @todo		Pages in shared memory areas should be marked as
 *			pageable.
 */

#include <lib/avl_tree.h>
#include <lib/refcount.h>

#include <mm/page.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <public/shm.h>

#include <sync/mutex.h>
#include <sync/rwlock.h>

#include <errors.h>
#include <init.h>
#include <object.h>
#include <vmem.h>

/** Structure containing details of a shared memory area. */
typedef struct shm {
	object_t obj;			/**< Object header. */

	shm_id_t id;			/**< ID of the area. */
	size_t size;			/**< Size of area. */
	mutex_t lock;			/**< Lock to protect page tree. */
	avl_tree_t pages;		/**< Tree of pages. */
	refcount_t count;		/**< Number of references to the area. */
} shm_t;

/** Shared memory ID allocator. */
static vmem_t *shm_id_arena;

/** Slab cache for shared memory area structures. */
static slab_cache_t *shm_cache;

/** Tree containing shared memory areas. */
static AVL_TREE_DECLARE(shm_tree);
static RWLOCK_DECLARE(shm_tree_lock);

/** Constructor for shared memory area structures.
 * @param obj		Object to construct.
 * @param data		Unused.
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int shm_ctor(void *obj, void *data, int kmflag) {
	shm_t *area = obj;

	mutex_init(&area->lock, "shm_lock", 0);
	avl_tree_init(&area->pages);
	return 0;
}

/** Close a handle to a shared memory area.
 * @param handle	Handle to the area. */
static void shm_object_close(khandle_t *handle) {
	shm_t *area = (shm_t *)handle->object;
	vm_page_t *page;

	if(refcount_dec(&area->count) == 0) {
		/* Free all pages. */
		AVL_TREE_FOREACH_SAFE(&area->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			avl_tree_remove(&area->pages, page->offset);
			vm_page_free(page, 1);
		}

		rwlock_write_lock(&shm_tree_lock);
		avl_tree_remove(&shm_tree, area->id);
		rwlock_unlock(&shm_tree_lock);

		vmem_free(shm_id_arena, area->id, 1);
		object_destroy(&area->obj);
		slab_cache_free(shm_cache, area);
	}
}

/** Get a page from the object.
 * @param handle	Handle to object to get page from.
 * @param offset	Offset into object to get page from.
 * @param physp		Where to store physical address of page.
 * @return		0 on success, negative error code on failure. */
static int shm_object_get_page(khandle_t *handle, offset_t offset, phys_ptr_t *physp) {
	shm_t *area = (shm_t *)handle->object;
	vm_page_t *page;

	mutex_lock(&area->lock);

	/* Ensure that the requested page is within the area. */
	if(offset >= (offset_t)area->size) {
		mutex_unlock(&area->lock);
		return -ERR_ADDR_INVAL;
	}

	/* If the page is not already in the object, allocate a new page. */
	if(!(page = avl_tree_lookup(&area->pages, offset))) {
		page = vm_page_alloc(1, MM_SLEEP | PM_ZERO);
		page->offset = offset;
		avl_tree_insert(&area->pages, offset, page, NULL);
	}

	*physp = page->addr;
	mutex_unlock(&area->lock);
	return 0;
}

/** Shared memory object type. */
static object_type_t shm_object_type = {
	.id = OBJECT_TYPE_SHM,
	.close = shm_object_close,
	.get_page = shm_object_get_page,
};

/** Create a new shared memory area.
 * @param size		Size of the area (multiple of system page size).
 * @return		Handle to area on success, negative error code on
 *			failure. */
handle_t sys_shm_create(size_t size) {
	khandle_t *handle;
	handle_t ret;
	shm_t *area;

	if(size == 0 || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	}

	area = slab_cache_alloc(shm_cache, MM_SLEEP);
	if(!(area->id = vmem_alloc(shm_id_arena, 1, 0))) {
		slab_cache_free(shm_cache, area);
		return -ERR_RESOURCE_UNAVAIL;
	}

	object_init(&area->obj, &shm_object_type);
	refcount_set(&area->count, 1);
	area->size = size;

	handle_create(&area->obj, NULL, NULL, 0, &handle);
	ret = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	if(ret < 0) {
		/* The handle_release() call frees the area. */
		return ret;
	}

	rwlock_write_lock(&shm_tree_lock);
	avl_tree_insert(&shm_tree, area->id, area, NULL);
	rwlock_unlock(&shm_tree_lock);
	return ret;
}

/** Open a handle to a shared memory area.
 * @param id		ID of area to open.
 * @return		Handle to area on success, negative error code on
 *			failure. */
handle_t sys_shm_open(shm_id_t id) {
	khandle_t *handle;
	handle_t ret;
	shm_t *area;

	rwlock_read_lock(&shm_tree_lock);

	if(!(area = avl_tree_lookup(&shm_tree, id))) {
		rwlock_unlock(&shm_tree_lock);
		return -ERR_NOT_FOUND;
	}

	refcount_inc(&area->count);
	rwlock_unlock(&shm_tree_lock);

	handle_create(&area->obj, NULL, NULL, 0, &handle);
	ret = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	return ret;
}

/** Get the ID of a shared memory area.
 * @param handle	Handle to area.
 * @return		ID of area on success, negative error code on failure. */
shm_id_t sys_shm_id(handle_t handle) {
	khandle_t *khandle;
	shm_id_t ret;
	shm_t *area;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_SHM, &khandle)) != 0) {
		return ret;
	}

	area = (shm_t *)khandle->object;
	ret = area->id;
	handle_release(khandle);
	return ret;
}

/** Resize a shared memory area.
 * @todo		Support shrinking areas.
 * @param handle	Handle to area.
 * @param size		New size of the area.
 * @return		0 on success, negative error code on failure. */
int sys_shm_resize(handle_t handle, size_t size) {
	khandle_t *khandle;
	shm_t *area;
	int ret;

	if(size == 0 || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	}

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_SHM, &khandle)) != 0) {
		return ret;
	}

	area = (shm_t *)khandle->object;
	if(size < area->size) {
		ret = -ERR_NOT_IMPLEMENTED;
	} else {
		area->size = size;
	}

	handle_release(khandle);
	return ret;
}

/** Initialise the shared memory code. */
static void __init_text shm_init(void) {
	shm_id_arena = vmem_create("shm_id_arena", 1, 65535, 1, NULL, NULL, NULL, 0, 0, MM_FATAL);
	shm_cache = slab_cache_create("shm_cache", sizeof(shm_t), 0, shm_ctor,
	                              NULL, NULL, NULL, SLAB_DEFAULT_PRIORITY,
	                              NULL, 0, MM_FATAL);
}
INITCALL(shm_init);
