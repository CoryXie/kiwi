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
 * @brief		Bitmap data type.
 */

#ifndef __LIB_BITMAP_H
#define __LIB_BITMAP_H

#include <lib/utility.h>

#include <mm/flags.h>

/** Structure containing a bitmap. */
typedef struct bitmap {
	uint8_t *data;		/**< Bitmap data. */
	int count;		/**< Number of bits in the bitmap. */
	bool allocated;		/**< Whether data was allocated by bitmap_init(). */
} bitmap_t;

/** Get the number of bytes required for a bitmap. */
#define BITMAP_BYTES(bits)	(ROUND_UP(bits, 8) / 8)

extern status_t bitmap_init(bitmap_t *bitmap, int bits, uint8_t *data, int mmflag);
extern void bitmap_destroy(bitmap_t *bitmap);

extern void bitmap_set(bitmap_t *bitmap, int bit);
extern void bitmap_clear(bitmap_t *bitmap, int bit);
extern bool bitmap_test(bitmap_t *bitmap, int bit);
extern int bitmap_ffs(bitmap_t *bitmap);
extern int bitmap_ffz(bitmap_t *bitmap);

#endif /* __LIB_BITMAP_H */
