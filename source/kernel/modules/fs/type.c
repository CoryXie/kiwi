/* Kiwi VFS filesystem type management
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
 * @brief		VFS filesystem type management.
 */

#include <lib/string.h>

#include <errors.h>
#include <init.h>
#include <module.h>

#include "vfs_priv.h"

/** List of registered VFS types. */
static LIST_DECLARE(vfs_type_list);
static MUTEX_DECLARE(vfs_type_list_lock);

/** Look up a filesystem type with lock already held.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static vfs_type_t *vfs_type_lookup_internal(const char *name) {
	vfs_type_t *type;

	LIST_FOREACH(&vfs_type_list, iter) {
		type = list_entry(iter, vfs_type_t, header);

		if(strcmp(type->name, name) == 0) {
			return type;
		}
	}

	return NULL;
}

/** Look up a filesystem type.
 * @param name		Name of filesystem type to look up.
 * @param ref		Whether to add a reference to the type.
 * @return		Pointer to type structure if found, NULL if not. */
vfs_type_t *vfs_type_lookup(const char *name, bool ref) {
	vfs_type_t *type;

	mutex_lock(&vfs_type_list_lock, 0);

	type = vfs_type_lookup_internal(name);
	if(type && ref) {
		refcount_inc(&type->count);
	}

	mutex_unlock(&vfs_type_list_lock);
	return type;
}

/** Register a new filesystem type.
 *
 * Registers a new filesystem type with the VFS.
 *
 * @param type		Pointer to type structure to register.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_type_register(vfs_type_t *type) {
	mutex_lock(&vfs_type_list_lock, 0);

	/* Check if this type already exists. */
	if(vfs_type_lookup_internal(type->name) != NULL) {
		return -ERR_OBJ_EXISTS;
	}

	list_init(&type->header);
	list_append(&vfs_type_list, &type->header);

	dprintf("vfs: registered filesystem type 0x%p(%s)\n", type, type->name);
	mutex_unlock(&vfs_type_list_lock);
	return 0;
}
MODULE_EXPORT(vfs_type_register);

/** Remove a filesystem type.
 *
 * Removes a previously registered filesystem type from the list of
 * filesystem types.
 *
 * @param type		Type to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_type_unregister(vfs_type_t *type) {
	return -ERR_NOT_IMPLEMENTED;
}
MODULE_EXPORT(vfs_type_unregister);