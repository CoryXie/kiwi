/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Array search function.
 */

#include <stdlib.h>

/** Search a sorted array.
 *
 * Searches a sorted array of items for the given key.
 *
 * @param key		Key to search for.
 * @param base		Start of the array.
 * @param nmemb		Number of array elements.
 * @param size		Size of each array element.
 * @param compar	Comparison function.
 *
 * @return		Pointer to found key, or NULL if not found.
 */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	size_t low;
	size_t mid;
	char *p;
	int r;

	if(size > 0) {
		low = 0;
		while(low < nmemb) {
			mid = low + ((nmemb - low) >> 1);
			p = ((char *)base) + mid * size;
			r = (*compar)(key, p);
			if(r > 0) {
				low = mid + 1;
			} else if(r < 0) {
				nmemb = mid;
			} else {
				return p;
			}
		}
	}

	return NULL;
}