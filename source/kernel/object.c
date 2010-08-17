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
 * @brief		Kernel object manager.
 *
 * The kernel object manager manages all userspace-accessible objects. It
 * allows processes (as well as the kernel) to create handles to objects, and
 * implements an access control list that limits which processes can access
 * objects.
 *
 * It does not, however, manage how objects are referred to (i.e. there isn't
 * a single namespace for all objects - for example FS entries are referred to
 * by path strings, but ports, memory areas, etc. are referred to by global
 * IDs), or the lifetime of objects - it is up to each object type to manage
 * these.
 *
 * @todo		Implement the ACL.
 */

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <sync/semaphore.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>
#include <object.h>
#include <status.h>

#if CONFIG_OBJECT_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Object waiting synchronization information structure. */
typedef struct object_wait_sync {
	semaphore_t sem;		/**< Semaphore counting events. */
	object_wait_t *event;		/**< Handle that received first event (atomically set). */
} object_wait_sync_t;

/** Structure linking a handle to a handle table. */
typedef struct handle_link {
	khandle_t *handle;		/**< Handle the link refers to. */
	int flags;			/**< Behaviour flags for the handle. */
} handle_link_t;

/** Cache for handle structures. */
static slab_cache_t *khandle_cache;

/** Cache for handle table structures. */
static slab_cache_t *handle_table_cache;

/** Constructor for handle table objects.
 * @param obj		Object to construct.
 * @param data		Cache data pointer. */
static void handle_table_ctor(void *obj, void *data) {
	handle_table_t *table = obj;

	avl_tree_init(&table->tree);
	rwlock_init(&table->lock, "handle_table_lock");
}

/** Initialise an object structure.
 * @param obj		Object to initialise.
 * @param type		Pointer to type structure for object type. Can be NULL,
 *			in which case the object will be a 'NULL object', and
 *			handles will never be created to it. */
void object_init(object_t *obj, object_type_t *type) {
	obj->type = type;
}

/** Destroy an object structure.
 * @param obj		Object to destroy. */
void object_destroy(object_t *obj) {
	/* TODO: Clean ACL. */
}

/** Notifier function to use for object waiting.
 * @param arg1		Unused.
 * @param arg2		Unused.
 * @param arg3		Wait structure pointer. */
void object_wait_notifier(void *arg1, void *arg2, void *arg3) {
	object_wait_callback(arg3);
}

/** Object waiting callback function.
 * @param wait		Wait information structure. */
void object_wait_callback(object_wait_t *wait) {
	object_wait_sync_t *sync = wait->priv;

	__sync_bool_compare_and_swap(&sync->event, NULL, wait);
	semaphore_up(&sync->sem, 1);
}

/** Wait for an event to happen on an object.
 * @param handle	Handle to wait on.
 * @param event		Event ID to wait for (specific to object type).
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			cause the function to return immediately if the event
 *			has not happened, and a value of -1 will block
 *			indefinitely until the event happens.
 * @return		Status code describing result of the operation. */
status_t object_wait(khandle_t *handle, int event, useconds_t timeout) {
	object_wait_sync_t sync;
	object_wait_t wait;
	status_t ret;

	if(!handle) {
		return STATUS_INVALID_ARG;
	}

	semaphore_init(&sync.sem, "object_wait_sem", 0);
	sync.event = NULL;
	wait.handle = handle;
	wait.event = event;
	wait.priv = &sync;

	if(!handle->object->type->wait || !handle->object->type->unwait) {
		return STATUS_INVALID_EVENT;
	}

	ret = handle->object->type->wait(&wait);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = semaphore_down_etc(&sync.sem, timeout, SYNC_INTERRUPTIBLE);
	handle->object->type->unwait(&wait);
	return ret;
}

/** Wait for events to happen on multiple objects.
 * @param handles	Array of handles to wait for.
 * @param event		Array of event IDs to wait for (specific to object
 *			type). The index into the array selects the handle the
 *			event is for - for example, specifying 1 as the event
 *			at index 1 will wait for event 1 on the handle at index
 *			1 in the handle array. If you wish to wait for multiple
 *			events on one handle, specify the handle multiple times
 *			in the arrays.
 * @param count		Number of handles.
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			cause the function to return immediately if the event
 *			has not happened, and a value of -1 will block
 *			indefinitely until the event happens.
 * @param indexp	Where to store index of event that occurred.
 * @return		Status code describing result of the operation. */
status_t object_wait_multiple(khandle_t **handles, int *events, size_t count, useconds_t timeout,
                              int *indexp) {
	object_wait_sync_t sync;
	object_wait_t *waits;
	status_t ret;
	size_t i;

	if(!count || count > 1024 || !handles || !events) {
		return STATUS_INVALID_ARG;
	}

	semaphore_init(&sync.sem, "object_wait_sem", 0);
	sync.event = NULL;

	/* Allocate wait structures for each handle and fill them in. */
	waits = kmalloc(sizeof(object_wait_t) * count, MM_SLEEP);
	for(i = 0; i < count; i++) {
		if(!handles[i]) {
			ret = STATUS_INVALID_ARG;
			goto out;
		} else if(!handles[i]->object->type->wait || !handles[i]->object->type->unwait) {
			ret = STATUS_INVALID_EVENT;
			goto out;
		}

		waits[i].handle = handles[i];
		waits[i].event = events[i];
		waits[i].priv = &sync;
		waits[i].idx = i;

		ret = handles[i]->object->type->wait(&waits[i]);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}

	/* Wait for any of the events to be signalled. */
	ret = semaphore_down_etc(&sync.sem, timeout, SYNC_INTERRUPTIBLE);
	if(ret == STATUS_SUCCESS) {
		assert(sync.event);
		if(indexp) {
			*indexp = sync.event->idx;
		}
	}
out:
	while(i--) {
		handles[i]->object->type->unwait(&waits[i]);
	}
	kfree(waits);
	return ret;
}

/** Create a handle to an object.
 *
 * Creates a new handle to an object. This handle will not be attached to any
 * process: it can later be attached using handle_attach(). Alternatively,
 * handle_create_and_attach() can be used.
 *
 * @param obj		Object to create a handle to.
 * @param data		Per-handle data pointer.
 *
 * @return		Pointer to handle structure created.
 */
khandle_t *handle_create(object_t *obj, void *data) {
	khandle_t *handle;

	assert(obj);
	assert(obj->type);

	handle = slab_cache_alloc(khandle_cache, MM_SLEEP);
	refcount_set(&handle->count, 1);
	handle->object = obj;
	handle->data = data;
	return handle;
}

/** Create a handle to an object in a process.
 * @param process	Process to attach to.
 * @param obj		Object to create a handle to.
 * @param data		Per-handle data pointer.
 * @param flags		Flags for the handle table entry.
 * @param idp		Where to store ID of handle created.
 * @param uidp		If not NULL, a user-mode pointer to copy handle ID to.
 * @return		Status code describing result of the operation. */
status_t handle_create_and_attach(process_t *process, object_t *obj, void *data, int flags,
                                  handle_t *idp, handle_t *uidp) {
	khandle_t *handle = handle_create(obj, data);
	status_t ret;

	ret = handle_attach(process, handle, flags, idp, uidp);
	if(ret == STATUS_SUCCESS) {
		handle_release(handle);
	} else {
		/* We do not want the object's close operation to be called
		 * upon failure, so free the handle directly. */
		slab_cache_free(khandle_cache, handle);
	}

	return ret;
}

/** Increase the reference count of a handle.
 *
 * Increases the reference count of a handle to signal that it is being used.
 * When the handle is no longer needed it should be released with
 * handle_release().
 * 
 * @param handle	Handle to increase reference count of.
 */
void handle_get(khandle_t *handle) {
	assert(handle);
	refcount_inc(&handle->count);
}

/** Release a handle.
 *
 * Decreases the reference count of a handle. If no more references remain to
 * the handle, it will be destroyed.
 *
 * @param handle	Handle to release.
 */
void handle_release(khandle_t *handle) {
	assert(handle);

	/* If there are no more references we can close it. */
	if(refcount_dec(&handle->count) == 0) {
		if(handle->object->type->close) {
			handle->object->type->close(handle);
		}
		slab_cache_free(khandle_cache, handle);
	}
}

/** Insert a handle into a handle table.
 * @param table		Table to insert into.
 * @param id		ID to give the handle.
 * @param handle	Handle to insert.
 * @param flags		Flags for the handle. */
static void handle_table_insert(handle_table_t *table, handle_t id, khandle_t *handle, int flags) {
	handle_link_t *link;

	handle_get(handle);

	link = kmalloc(sizeof(handle_link_t), MM_SLEEP);
	link->handle = handle;
	link->flags = flags;

	bitmap_set(&table->bitmap, id);
	avl_tree_insert(&table->tree, id, link, NULL);
}

/** Insert a handle into a process' handle table.
 *
 * Allocates a handle ID for a process and adds a handle to it. On success,
 * the handle will have an extra reference on it.
 *
 * @param process	Process to attach to.
 * @param handle	Handle to attach.
 * @param flags		Flags for the handle.
 * @param idp		Where to store ID of handle created.
 * @param uidp		If not NULL, a user-mode pointer to copy handle ID to.
 *
 * @return		Status code describing result of the operation.
 */
status_t handle_attach(process_t *process, khandle_t *handle, int flags, handle_t *idp, handle_t *uidp) {
	status_t ret;
	handle_t id;

	assert(process);
	assert(handle);
	assert(idp || uidp);

	rwlock_write_lock(&process->handles->lock);

	/* Find a handle ID in the table. */
	id = bitmap_ffz(&process->handles->bitmap);
	if(id < 0) {
		rwlock_unlock(&process->handles->lock);
		return STATUS_NO_HANDLES;
	}

	/* Copy the handle ID before actually attempting to insert so we don't
	 * have to detach if it fails. */
	if(uidp) {
		ret = memcpy_to_user(uidp, &id, sizeof(handle_t));
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&process->handles->lock);
			return ret;
		}
	}

	handle_table_insert(process->handles, id, handle, flags);
	rwlock_unlock(&process->handles->lock);

	dprintf("object: allocated handle %d in process %" PRId32 " (object: %p, data: %p)\n",
	        id, process->id, handle->object, handle->data);
	if(idp) {
		*idp = id;
	}
	return STATUS_SUCCESS;
}

/** Detach a handle from a process.
 *
 * Removes the specified handle ID from a process' handle table and releases
 * the handle.
 *
 * @param process	Process to remove from.
 * @param id		ID of handle to detach.
 *
 * @return		Status code describing result of the operation.
 */
status_t handle_detach(process_t *process, handle_t id) {
	handle_link_t *link;

	assert(process);

	rwlock_write_lock(&process->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&process->handles->tree, (key_t)id);
	if(!link) {
		rwlock_unlock(&process->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Remove from the tree and mark the ID as free. */
	avl_tree_remove(&process->handles->tree, (key_t)id);
	bitmap_clear(&process->handles->bitmap, id);

	/* Release the handle and free the link. */
	handle_release(link->handle);
	kfree(link);

	dprintf("object: detached handle %" PRId32 " from process %" PRId32 "\n",
	        id, process->id);
	rwlock_unlock(&process->handles->lock);
	return STATUS_SUCCESS;
}

/** Look up a handle in a process' handle table.
 *
 * Looks up the handle with the given ID in a process' handle table, ensuring
 * that the object is a certain type. The returned handle will have an extra
 * reference on it - when it is no longer needed, it should be released with
 * handle_release().
 *
 * @param process	Process to look up in.
 * @param id		Handle ID to look up.
 * @param type		Required object type ID (if negative, no type checking
 *			will be performed).
 * @param handlep	Where to store pointer to handle structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t handle_lookup(process_t *process, handle_t id, int type, khandle_t **handlep) {
	handle_link_t *link;

	assert(process);
	assert(handlep);

	rwlock_read_lock(&process->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&process->handles->tree, (key_t)id);
	if(!link) {
		rwlock_unlock(&process->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Check if the type is the type the caller wants. */
	if(type >= 0 && link->handle->object->type->id != type) {
		rwlock_unlock(&process->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	handle_get(link->handle);
	*handlep = link->handle;
	rwlock_unlock(&process->handles->lock);
	return STATUS_SUCCESS;
}

/** Create a new handle table.
 *
 * Creates a new handle table and duplicates handles from another handle table
 * into it, either using provided mapping information, or by looking at the
 * inheritable flag of handles.
 *
 * @param parent	Parent process' handle table (can be NULL, in which
 *			case no handles will be duplicated).
 * @param map		An array specifying handles to add to the new table.
 *			The first ID of each entry specifies the handle in the
 *			parent, and the second specifies the ID to give it in
 *			the new table. Can be NULL if count <= 0.
 * @param count		The number of handles in the array. If negative, the
 *			map will be ignored and all handles with the inheritable
 *			flag set will be duplicated. If 0, no handles will be
 *			duplicated.
 * @param tablep	Where to store pointer to table structure.
 *
 * @return		Status code describing result of the operation. Failure
 *			can only occur when handle mappings are specified.
 */
status_t handle_table_create(handle_table_t *parent, handle_t map[][2], int count,
                             handle_table_t **tablep) {
	handle_table_t *table;
	handle_link_t *link;
	int i;

	table = slab_cache_alloc(handle_table_cache, MM_SLEEP);
	bitmap_init(&table->bitmap, CONFIG_HANDLE_MAX, NULL, MM_SLEEP);

	/* Inherit all inheritable handles in the parent table. */
	if(parent && count != 0) {
		rwlock_read_lock(&parent->lock);

		if(count > 0) {
			assert(map);

			for(i = 0; i < count; i++) {
				link = avl_tree_lookup(&parent->tree, map[i][0]);
				if(!link) {
					rwlock_unlock(&parent->lock);
					handle_table_destroy(table);
					return STATUS_INVALID_HANDLE;
				} else if(map[i][1] >= CONFIG_HANDLE_MAX) {
					rwlock_unlock(&parent->lock);
					handle_table_destroy(table);
					return STATUS_INVALID_HANDLE;
				} else if(avl_tree_lookup(&table->tree, map[i][1])) {
					rwlock_unlock(&parent->lock);
					handle_table_destroy(table);
					return STATUS_ALREADY_EXISTS;
				}

				handle_table_insert(table, map[i][1], link->handle, link->flags);
			}
		} else {
			AVL_TREE_FOREACH(&parent->tree, iter) {
				link = avl_tree_entry(iter, handle_link_t);

				if(link->flags & HANDLE_INHERITABLE) {
					handle_table_insert(table, iter->key, link->handle,
					                    link->flags);
				}
			}
		}

		rwlock_unlock(&parent->lock);
	}

	*tablep = table;
	return STATUS_SUCCESS;
}

/** Clone a handle table.
 *
 * Creates a clone of a handle table. All handles, even non-inheritable ones,
 * will be copied into the new table. The table entries will all refer to the
 * same underlying handle as the old table.
 *
 * @param src		Source table.
 *
 * @return		Pointer to cloned table.
 */
handle_table_t *handle_table_clone(handle_table_t *src) {
	handle_table_t *table;
	handle_link_t *link;

	table = slab_cache_alloc(handle_table_cache, MM_SLEEP);
	bitmap_init(&table->bitmap, CONFIG_HANDLE_MAX, NULL, MM_SLEEP);

	rwlock_read_lock(&src->lock);

	AVL_TREE_FOREACH(&src->tree, iter) {
		link = avl_tree_entry(iter, handle_link_t);
		handle_table_insert(table, iter->key, link->handle, link->flags);
	}

	rwlock_unlock(&src->lock);
	return table;
}

/** Destroy a handle table.
 * @param table		Table being destroyed. */
void handle_table_destroy(handle_table_t *table) {
	handle_link_t *link;

	/* Close all handles. */
	AVL_TREE_FOREACH_SAFE(&table->tree, iter) {
		link = avl_tree_entry(iter, handle_link_t);
		handle_release(link->handle);
		avl_tree_remove(&table->tree, iter->key);
		kfree(link);
	}

	bitmap_destroy(&table->bitmap);
	slab_cache_free(handle_table_cache, table);
}

/** Print a list of a process' handles.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_handles(int argc, char **argv) {
	handle_link_t *link;
	process_t *process;
	unative_t id;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <process ID>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out a list of all currently open handles in a process.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &id, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	} else if(!(process = process_lookup_unsafe(id))) {
		kprintf(LOG_NONE, "Invalid process ID.\n");
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID    Object             Type                  Count  Data\n");
	kprintf(LOG_NONE, "==    ======             ====                  =====  ====\n");

	AVL_TREE_FOREACH(&process->handles->tree, iter) {
		link = avl_tree_entry(iter, handle_link_t);
		kprintf(LOG_NONE, "%-5" PRIu64 " %-18p %d(%-18p) %-6d %p\n",
		        iter->key, link->handle->object, link->handle->object->type->id,
		        link->handle->object->type, refcount_get(&link->handle->count),
		        link->handle->data);
	}

	return KDBG_OK;
}

/** Initialise the handle caches. */
void __init_text handle_init(void) {
	khandle_cache = slab_cache_create("khandle_cache", sizeof(khandle_t), 0, NULL,
	                                  NULL, NULL, NULL, SLAB_DEFAULT_PRIORITY,
	                                  NULL, 0, MM_FATAL);
	handle_table_cache = slab_cache_create("handle_table_cache", sizeof(handle_table_t),
	                                       0, handle_table_ctor, NULL, NULL, NULL,
	                                       SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}

/** Get the type of an object referred to by a handle.
 * @param handle	Handle to object.
 * @return		Type ID of object on success, -1 if the handle was not
 *			found. */
int sys_object_type(handle_t handle) {
	khandle_t *khandle;
	int ret;

	if(handle_lookup(curr_proc, handle, -1, &khandle) != STATUS_SUCCESS) {
		return -1;
	}

	ret = khandle->object->type->id;
	handle_release(khandle);
	return ret;
}

/** Wait for an event to happen on an object.
 * @param handle	Handle ID to wait on.
 * @param event		Event ID to wait for (specific to object type).
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			cause the function to return immediately if the event
 *			has not happened, and a value of -1 will block
 *			indefinitely until the event happens.
 * @return		Status code describing result of the operation. */
status_t sys_object_wait(handle_t handle, int event, useconds_t timeout) {
	khandle_t *khandle;
	status_t ret;

	ret = handle_lookup(curr_proc, handle, -1, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = object_wait(khandle, event, timeout);
	handle_release(khandle);
	return ret;
}

/** Wait for events to happen on multiple objects.
 * @param handles	Array of handle IDs to wait for.
 * @param event		Array of event IDs to wait for (specific to object
 *			type). The index into the array selects the handle the
 *			event is for - for example, specifying 1 as the event
 *			at index 1 will wait for event 1 on the handle at index
 *			1 in the handle array. If you wish to wait for multiple
 *			events on one handle, specify the handle multiple times
 *			in the arrays.
 * @param count		Number of handles.
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			cause the function to return immediately if the event
 *			has not happened, and a value of -1 will block
 *			indefinitely until the event happens.
 * @param indexp	Where to store index of event that occurred.
 * @return		Status code describing result of the operation. */
status_t sys_object_wait_multiple(handle_t *handles, int *events, size_t count,
                                  useconds_t timeout, int *indexp) {
	handle_t *khandles = NULL;
	khandle_t **kobjs = NULL;
	int *kevents = NULL;
	status_t ret;
	int kindex;
	size_t i;

	if(!count || count > 1024 || !handles || !events) {
		return STATUS_INVALID_ARG;
	}

	khandles = kmalloc(sizeof(handle_t) * count, MM_SLEEP);
	ret = memcpy_from_user(khandles, handles, sizeof(handle_t) * count);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	kevents = kmalloc(sizeof(int) * count, MM_SLEEP);
	ret = memcpy_from_user(kevents, events, sizeof(int) * count);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	kobjs = kcalloc(count, sizeof(khandle_t *), MM_SLEEP);
	for(i = 0; i < count; i++) {
		ret = handle_lookup(curr_proc, khandles[i], -1, &kobjs[i]);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}

	ret = object_wait_multiple(kobjs, kevents, count, timeout, &kindex);
	if(ret == STATUS_SUCCESS && indexp) {
		ret = memcpy_to_user(indexp, &kindex, sizeof(int));
	}
out:
	if(kobjs) {
		for(i = 0; i < count; i++) {
			if(!kobjs[i]) {
				break;
			}
			handle_release(kobjs[i]);
		}
	}
	if(kevents) {
		kfree(kevents);
	}
	if(khandles) {
		kfree(khandles);
	}
	return ret;
}

/** Get behaviour flags for a handle.
 * @param handle	ID of handle to get flags for.
 * @param flagsp	Where to store handle flags.
 * @return		Status code describing result of the operation. */
status_t sys_handle_get_flags(handle_t handle, int *flagsp) {
	handle_link_t *link;
	status_t ret;

	rwlock_read_lock(&curr_proc->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&curr_proc->handles->tree, (key_t)handle);
	if(!link) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	ret = memcpy_to_user(flagsp, &link->flags, sizeof(int));
	rwlock_unlock(&curr_proc->handles->lock);
	return ret;
}

/** Set behaviour flags for a handle.
 * @param handle	ID of handle to set flags for.
 * @param flags		Flags to set.
 * @return		Status code describing result of the operation. */
status_t sys_handle_set_flags(handle_t handle, int flags) {
	handle_link_t *link;

	rwlock_write_lock(&curr_proc->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&curr_proc->handles->tree, (key_t)handle);
	if(!link) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	link->flags = flags;
	rwlock_unlock(&curr_proc->handles->lock);
	return STATUS_SUCCESS;
}

/** Close a handle.
 * @param handle	Handle ID to close.
 * @return		Status code describing result of the operation.
 *			The only reason for failure is an invalid handle being
 *			specified. */
status_t sys_handle_close(handle_t handle) {
	return handle_detach(curr_proc, handle);
}
