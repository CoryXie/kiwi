/*
 * Copyright (C) 2010-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel object manager.
 */

#ifndef __OBJECT_H
#define __OBJECT_H

#include <kernel/object.h>

#include <lib/list.h>
#include <lib/refcount.h>

#include <sync/rwlock.h>

struct object_handle;
struct process;

/** Kernel object type structure. */
typedef struct object_type {
	int id;				/**< ID number for the type. */
	unsigned flags;			/**< Flags for objects of this type. */

	/** Close a handle to an object.
	 * @param handle	Handle to the object. */
	void (*close)(struct object_handle *handle);

	/** Signal that an object event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param wait		Internal data pointer to be passed to
	 *			object_wait_signal() or object_wait_notifier().
	 * @return		Status code describing result of the operation. */
	status_t (*wait)(struct object_handle *handle, unsigned event, void *wait);

	/** Stop waiting for an object.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param wait		Internal data pointer. */
	void (*unwait)(struct object_handle *handle, unsigned event, void *wait);

	/** Check if an object can be memory-mapped.
	 * @note		If this function is implemented, the get_page
	 *			operation MUST be implemented. If it is not,
	 *			then the object will be classed as mappable if
	 *			get_page is implemented.
	 * @param handle	Handle to object.
	 * @param flags		Mapping flags (VM_MAP_*).
	 * @return		STATUS_SUCCESS if can be mapped, status code
	 *			explaining why if not. */
	status_t (*mappable)(struct object_handle *handle, int flags);

	/** Get a page from the object.
	 * @param handle	Handle to object to get page from.
	 * @param offset	Offset into object to get page from.
	 * @param physp		Where to store physical address of page.
	 * @return		Status code describing result of the operation. */
	status_t (*get_page)(struct object_handle *handle, offset_t offset,
		phys_ptr_t *physp);

	/** Release a page from the object.
	 * @param handle	Handle to object to release page in.
	 * @param offset	Offset of page in object.
	 * @param phys		Physical address of page that was unmapped. */
	void (*release_page)(struct object_handle *handle, offset_t offset,
		phys_ptr_t phys);
} object_type_t;

/** Properties of an object type. */
#define OBJECT_TRANSFERRABLE	(1<<0)	/**< Objects can be inherited or transferred over IPC. */
#define OBJECT_SECURABLE	(1<<1)	/**< Objects are secured through an ACL. */

/** Structure defining a kernel object.
 * @note		This structure is intended to be embedded inside
 *			another structure for the object. */
typedef struct object {
	object_type_t *type;		/**< Type of the object. */
} object_t;

/** Structure containing a handle to a kernel object. */
typedef struct object_handle {
	object_t *object;		/**< Object that the handle refers to. */
	void *data;			/**< Per-handle data pointer. */
	object_rights_t rights;		/**< Access rights for the handle. */
	refcount_t count;		/**< References to the handle. */
} object_handle_t;

/** Table that maps IDs to handles (handle_t -> object_handle_t). */
typedef struct handle_table {
	rwlock_t lock;			/**< Lock to protect table. */
	object_handle_t **handles;	/**< Array of allocated handles. */
	uint32_t *flags;		/**< Array of entry flags. */
	unsigned long *bitmap;		/**< Bitmap for tracking free handle IDs. */
} handle_table_t;

extern void object_init(object_t *object, object_type_t *type);
extern void object_destroy(object_t *object);
extern object_rights_t object_rights(object_t *object, struct process *process);

extern void object_wait_notifier(void *arg1, void *arg2, void *arg3);
extern void object_wait_signal(void *wait, unsigned long data);

extern object_handle_t *object_handle_create(object_t *object, void *data,
	object_rights_t rights);
extern status_t object_handle_open(object_t *object, void *data, object_rights_t rights,
	object_handle_t **handlep);
extern void object_handle_retain(object_handle_t *handle);
extern void object_handle_release(object_handle_t *handle);

/** Check if a handle has a set of rights.
 * @param handle	Handle to check.
 * @param rights	Rights to check for.
 * @return		Whether the handle has the rights. */
static inline bool object_handle_rights(object_handle_t *handle, object_rights_t rights) {
	return ((handle->rights & rights) == rights);
}

extern status_t object_handle_lookup(handle_t id, int type, object_rights_t rights,
	object_handle_t **handlep);
extern status_t object_handle_attach(object_handle_t *handle, handle_t *idp,
	handle_t *uidp);
extern status_t object_handle_detach(handle_t id);

extern status_t handle_table_create(handle_table_t *parent, handle_t map[][2],
	int count, handle_table_t **tablep);
extern handle_table_t *handle_table_clone(handle_table_t *src);
extern void handle_table_destroy(handle_table_t *table);

extern void handle_init(void);

#endif /* __OBJECT_H */
