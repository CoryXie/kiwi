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
 * @brief		Object functions.
 */

#include <kernel/object.h>
#include <kernel/status.h>

#include <stdlib.h>

#include "libkernel.h"

extern status_t _object_security(handle_t handle, user_id_t *uidp, group_id_t *gidp,
                                 object_acl_t *aclp);

/** Obtain object security attributes.
 * @param handle	Handle to object to get attributes for.
 * @param securityp	Security structure to fill in. Memory is allocated for
 *			data within this structure, which means it must be
 *			freed with object_security_destroy() once it is no
 *			longer needed.
 * @return		Status code describing result of the operation. */
__export status_t object_security(handle_t handle, object_security_t *securityp) {
	status_t ret;

	securityp->acl = malloc(sizeof(object_acl_t));
	if(!securityp->acl) {
		return STATUS_NO_MEMORY;
	}

	/* Call with a NULL entries pointer in order to get the size of the ACL.
	 * TODO: What if the ACL is changed between the two calls? */
	securityp->acl->entries = NULL;
	ret = _object_security(handle, &securityp->uid, &securityp->gid, securityp->acl);
	if(ret != STATUS_SUCCESS) {
		free(securityp->acl);
		return ret;
	}

	securityp->acl->entries = malloc(sizeof(object_acl_entry_t) * securityp->acl->count);
	if(!securityp->acl->entries) {
		free(securityp->acl);
		return STATUS_NO_MEMORY;
	}

	/* Get the ACL entries. */
	ret = _object_security(handle, NULL, NULL, securityp->acl);
	if(ret != STATUS_SUCCESS) {
		free(securityp->acl->entries);
		free(securityp->acl);
		return ret;
	}

	return STATUS_SUCCESS;
}

/** Get the ACL from an object security structure.
 * @param security	Structure to get from.
 * @return		Pointer to ACL, or NULL if failed to allocate one. */
__export object_acl_t *object_security_acl(object_security_t *security) {
	if(!security->acl) {
		security->acl = malloc(sizeof(object_acl_t));
		if(!security->acl) {
			return NULL;
		}
	}

	return security->acl;
}

/** Free memory allocated for an object security structure.
 * @param security	Structure to free data for. */
__export void object_security_destroy(object_security_t *security) {
	if(security->acl) {
		if(security->acl->entries) {
			free(security->acl->entries);
			security->acl->entries = NULL;
		}
		free(security->acl);
		security->acl = NULL;
	}
}
