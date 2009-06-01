/* Kiwi radix tree implementation
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
 * @brief		Radix tree implementation.
 */

#ifndef __TYPES_RADIX_H
#define __TYPES_RADIX_H

#include <types.h>

/** Radix tree node structure. */
typedef struct radix_tree_node {
	unsigned char *key;		/**< Key for this node. */
	void *value;			/**< Node value. */
	size_t child_count;		/**< Number of child nodes. */

	/** Pointer to parent node. */
	struct radix_tree_node *parent;

	/** Array of child nodes. Big enough to store all values of char. */
	struct radix_tree_node *children[256];
} radix_tree_node_t;

/** Radix tree structure. */
typedef struct radix_tree {
	radix_tree_node_t root;		/**< Root node. */
} radix_tree_t;

extern void radix_tree_insert(radix_tree_t *tree, const char *key, void *value);
extern void radix_tree_remove(radix_tree_t *tree, const char *key);
extern void *radix_tree_lookup(radix_tree_t *tree, const char *key);

extern void radix_tree_init(radix_tree_t *tree);
extern void radix_tree_destroy(radix_tree_t *tree);

extern void radix_tree_dump(radix_tree_t *tree);

#endif /* __TYPES_RADIX_H */
