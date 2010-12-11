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
 * @brief		POSIX change file mode function.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>

#include "posix_priv.h"

/** Convert a mode to a set of rights.
 * @param mode		Mode to convert (the part of interest should be in the
 *			lowest 3 bits).
 * @return		Converted rights. */
static inline object_rights_t mode_to_rights(uint16_t mode) {
	object_rights_t rights = 0;

	if(mode & S_IROTH) {
		rights |= FILE_READ;
	}
	if(mode & S_IWOTH) {
		rights |= FILE_WRITE;
	}
	if(mode & S_IXOTH) {
		rights |= FILE_EXECUTE;
	}
	return rights;
}

/** Convert a POSIX file mode to a kernel ACL.
 * @param current	If not NULL, the current ACL. Entries not supported by
 *			POSIX (user and group entries for specific users) will
 *			be preserved in it.
 * @param mode		Mode to convert.
 * @return		Pointer to ACL on success, NULL on failure. */
object_acl_t *posix_mode_to_acl(object_acl_t *exist, mode_t mode) {
	object_rights_t rights;
	object_acl_t *acl;
	size_t i;

	if(exist) {
		acl = exist;

		/* Clear out any entries we're going to be modifying. */
		for(i = 0; i < acl->count; i++) {
			switch(acl->entries[i].type) {
			case ACL_ENTRY_USER:
				if(acl->entries[i].value < 0) {
					acl->entries[i].rights = 0;
				}
				break;
			case ACL_ENTRY_GROUP:
				if(acl->entries[i].value < 0) {
					acl->entries[i].rights = 0;
				}
				break;
			case ACL_ENTRY_OTHERS:
				acl->entries[i].rights = 0;
				break;
			}
		}
	} else {
		acl = malloc(sizeof(*acl));
		if(!acl) {
			return NULL;
		}

		object_acl_init(acl);
	}

	/* Add in the rights specified by the mode. */
	rights = mode_to_rights((mode & S_IRWXU) >> 6);
	if(object_acl_add_entry(acl, ACL_ENTRY_USER, -1, rights) != STATUS_SUCCESS) {
		goto fail;
	}
	rights = mode_to_rights((mode & S_IRWXG) >> 3);
	if(object_acl_add_entry(acl, ACL_ENTRY_GROUP, -1, rights) != STATUS_SUCCESS) {
		goto fail;
	}
	rights = mode_to_rights(mode & S_IRWXO);
	if(object_acl_add_entry(acl, ACL_ENTRY_OTHERS, 0, rights) != STATUS_SUCCESS) {
		goto fail;
	}

	return acl;
fail:
	if(!exist) {
		object_acl_destroy(acl);
		free(acl);
	}
	errno = ENOMEM;
	return NULL;
}

/** Change a file's mode.
 * @param path		Path to file.
 * @param mode		New mode for the file.
 * @return		0 on success, -1 on failure. */
int chmod(const char *path, mode_t mode) {
	object_security_t security;
	status_t ret;

	/* Get the current security attributes, as we want to preserve extra
	 * ACL entries. */
	ret = kern_fs_security(path, true, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	/* Convert the mode to an ACL. */
	if(!posix_mode_to_acl(security.acl, mode)) {
		object_security_destroy(&security);
		return -1;
	}

	security.uid = -1;
	security.gid = -1;

	/* Set the new security attributes. */
	ret = kern_fs_set_security(path, true, &security);
	object_security_destroy(&security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Change a file's mode.
 * @param fd		File descriptor to file.
 * @param mode		New mode for the file.
 * @return		0 on success, -1 on failure. */
int fchmod(int fd, mode_t mode) {
	object_security_t security;
	status_t ret;

	/* Get the current security attributes, as we want to preserve extra
	 * ACL entries. */
	ret = kern_object_security(fd, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	/* Convert the mode to an ACL. */
	if(!posix_mode_to_acl(security.acl, mode)) {
		object_security_destroy(&security);
		return -1;
	}

	security.uid = -1;
	security.gid = -1;

	/* Set the new security attributes. */
	ret = kern_object_set_security(fd, &security);
	object_security_destroy(&security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
