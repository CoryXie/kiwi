/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Safe user memory access functions.
 */

#include <arch/memory.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <status.h>

/** Check if an address is valid. */
#if USER_MEMORY_BASE == 0
# define VALID(addr, count)		\
	(((addr) + (count)) <= USER_MEMORY_SIZE && ((addr) + (count)) >= (addr))
#else
# define VALID(addr, count)			\
	((addr) >= USER_MEMORY_BASE && ((addr) + (count)) <= (USER_MEMORY_BASE + USER_MEMORY_SIZE) && ((addr) + (count)) >= (addr))
#endif

/** Common entry code for userspace memory functions. */
#define USERMEM_ENTER()			\
	if(context_save(&curr_thread->usermem_context)) { \
		return STATUS_INVALID_ADDR; \
	} \
	curr_thread->in_usermem = true

/** Common exit code for userspace memory functions. */
#define USERMEM_EXIT(status)		\
	curr_thread->in_usermem = false; \
	return (status)

/** Return success from a userspace memory function. */
#define USERMEM_SUCCESS()		USERMEM_EXIT(STATUS_SUCCESS)

/** Return failure from a userspace memory function. */
#define USERMEM_FAIL()			USERMEM_EXIT(STATUS_INVALID_ADDR)

/** Code to check parameters execute a statement. */
#define USERMEM_WRAP(addr, count, stmt)	\
	if(!VALID((ptr_t)addr, count)) { \
		return STATUS_INVALID_ADDR; \
	} \
	USERMEM_ENTER(); \
	stmt; \
	USERMEM_SUCCESS()

/** Check if an address range points with userspace memory.
 * @note		Does not check if the memory is actually accessible!
 * @param dest		Base address.
 * @param size		Size of range.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on failure. */
status_t validate_user_address(void *dest, size_t size) {
	return (VALID((ptr_t)dest, size)) ? STATUS_SUCCESS : STATUS_INVALID_ADDR;
}

/** Copy data from userspace.
 *
 * Copies data from a userspace source memory area to a kernel memory area.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on failure.
 */
status_t memcpy_from_user(void *dest, const void *src, size_t count) {
	USERMEM_WRAP(src, count, memcpy(dest, src, count));
}

/** Copy data to userspace.
 *
 * Copies data from a kernel memory area to a userspace memory area.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on failure.
 */
status_t memcpy_to_user(void *dest, const void *src, size_t count) {
	USERMEM_WRAP(dest, count, memcpy(dest, src, count));
}

/** Fill a userspace memory area.
 *
 * Fills a userspace memory area with the value specified.
 *
 * @param dest		The memory area to fill.
 * @param val		The value to fill with.
 * @param count		The number of bytes to fill.
 *
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *			failure.
 */
status_t memset_user(void *dest, int val, size_t count) {
	USERMEM_WRAP(dest, count, memset(dest, val, count));
}

/** Get length of userspace string.
 *
 * Gets the length of the specified string residing in a userspace memory
 * area. The length is the number of characters found before a NULL byte.
 *
 * @param str		Pointer to the string.
 * @param lenp		Where to store string length.
 * 
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *			failure.
 */
status_t strlen_user(const char *str, size_t *lenp) {
	size_t retval = 0;

	USERMEM_ENTER();

	/* Yeah... this is horrible. */
	while(true) {
		if(!VALID((ptr_t)str, retval + 1)) {
			USERMEM_FAIL();
		} else if(str[retval] == 0) {
			break;
		}

		retval++;
	}

	*lenp = retval;
	USERMEM_SUCCESS();
}

/** Duplicate string from userspace.
 *
 * Allocates a buffer large enough and copies across a string from userspace.
 * The allocation is not made using MM_SLEEP, as there is no length limit and
 * therefore the length could be too large to fit in the heap. Use of
 * strndup_from_user() is preferred to this.
 *
 * @param src		Location to copy from.
 * @param destp		Pointer to buffer in which to store destination.
 *
 * @return		Status code describing result of the operation.
 *			Returns STATUS_INVALID_ARG if the string is
 *			zero-length.
 */
status_t strdup_from_user(const void *src, char **destp) {
	status_t ret;
	size_t len;
	char *d;

	ret = strlen_user(src, &len);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(len == 0) {
		return STATUS_INVALID_ARG;
	}

	d = kmalloc(len + 1, 0);
	if(!d) {
		return STATUS_NO_MEMORY;
	}

	ret = memcpy_from_user(d, src, len);
	if(ret != STATUS_SUCCESS) {
		kfree(d);
		return ret;
	}
	d[len] = 0;
	*destp = d;
	return STATUS_SUCCESS;
}

/** Duplicate string from userspace.
 *
 * Allocates a buffer large enough and copies across a string from userspace.
 * If the string is longer than the maximum length, then an error will be
 * returned. Because a length limit is provided, the allocation is made using
 * MM_SLEEP - it is assumed that the limit is sensible.
 *
 * @param src		Location to copy from.
 * @param max		Maximum length allowed.
 * @param destp		Pointer to buffer in which to store destination.
 *
 * @return		Status code describing result of the operation.
 *			Returns STATUS_INVALID_ARG if the string is
 *			zero-length.
 */
status_t strndup_from_user(const void *src, size_t max, char **destp) {
	status_t ret;
	size_t len;
	char *d;

	ret = strlen_user(src, &len);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(len == 0) {
		return STATUS_INVALID_ARG;
	} else if(len > max) {
		return STATUS_TOO_LONG;
	}

	d = kmalloc(len + 1, MM_SLEEP);
	ret = memcpy_from_user(d, src, len);
	if(ret != STATUS_SUCCESS) {
		kfree(d);
		return ret;
	}
	d[len] = 0;
	*destp = d;
	return STATUS_SUCCESS;
}

/** Copy a NULL-terminated array of strings from userspace.
 *
 * Copies a NULL-terminated array of strings from userspace. The array
 * itself and each array entry must be freed with kfree() once no longer
 * needed.
 *
 * @param src		Array to copy.
 * @param arrayp	Pointer to set to new array location.
 *
 * @return		Status code describing result of the operation.
 */
status_t arrcpy_from_user(const char *const src[], char ***arrayp) {
	char **array = NULL, **narr;
	status_t ret;
	int i;

	/* Copy the arrays across. */
	for(i = 0; ; i++) {
		narr = krealloc(array, sizeof(char *) * (i + 1), 0);
		if(!narr) {
			ret = STATUS_NO_MEMORY;
			goto fail;
		}

		array = narr;
		array[i] = NULL;

		ret = memcpy_from_user(&array[i], &src[i], sizeof(char *));
		if(ret != STATUS_SUCCESS) {
			array[i] = NULL;
			goto fail;
		} else if(array[i] == NULL) {
			break;
		}

		ret = strdup_from_user(array[i], &array[i]);
		if(ret != STATUS_SUCCESS) {
			array[i] = NULL;
			goto fail;
		}
	}

	*arrayp = array;
	return STATUS_SUCCESS;
fail:
	if(array) {
		for(i = 0; array[i] != NULL; i++) {
			kfree(array[i]);
		}
		kfree(array);
	}
	return ret;
}
