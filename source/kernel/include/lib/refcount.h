/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Reference counting functions.
 *
 * This file provides a reference count type and functions to modify the type.
 * The reference count is implemented using an atomic variable, and therefore
 * is atomic.
 */

#ifndef __LIB_REFCOUNT_H
#define __LIB_REFCOUNT_H

#include <lib/atomic.h>
#include <kernel.h>

/** Type containing a reference count */
typedef atomic_t refcount_t;

/** Initializes a statically declared reference count. */
#define REFCOUNT_INITIALIZER(_initial)		_initial

/** Statically declares a new reference count. */
#define REFCOUNT_DECLARE(_var, _initial)	\
	refcount_t _var = REFCOUNT_INITIALIZER(_initial)

/** Increase a reference count.
 * @param ref		Reference count to increase.
 * @return		The new value of the count. */
static inline int refcount_inc(refcount_t *ref) {
	return atomic_inc(ref) + 1;
}

/**
 * Decrease a reference count.
 *
 * Atomically decreases the value of a reference count. If it goes below 0
 * then a fatal() call will be made.
 *
 * @param ref		Reference count to decrease.
 *
 * @return		The new value of the count.
 */
static inline int refcount_dec(refcount_t *ref) {
	int val = atomic_dec(ref) - 1;

	if(unlikely(val < 0)) {
		fatal("Reference count %p went negative", ref);
	}

	return val;
}

/**
 * Decrease a reference count.
 *
 * Atomically decreases the value of a reference count. If it goes below 0
 * then the specified function will be called with a pointer to the reference
 * count as a parameter.
 *
 * @param ref		Reference count to decrease.
 * @param func		Function to call if count goes negative.
 *
 * @return		The new value of the count.
 */
static inline int refcount_dec_func(refcount_t *ref, void (*func)(refcount_t *)) {
	int val = atomic_dec(ref) - 1;

	if(unlikely(val < 0)) {
		func(ref);
	}

	return val;
}

/** Get the value of a reference count.
 * @param ref		Reference count to get value of.
 * @return		The value of the count. */
static inline int refcount_get(refcount_t *ref) {
	return atomic_get(ref);
}

/** Set the value of a reference count.
 * @param ref		Reference count to set.
 * @param val		Value to set to. */
static inline void refcount_set(refcount_t *ref, int val) {
	atomic_set(ref, val);
}

#endif /* __LIB_REFCOUNT_H */
