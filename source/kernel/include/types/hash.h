/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Hash table implementation.
 */

#ifndef __TYPES_HASH_H
#define __TYPES_HASH_H

#include <types/list.h>

#include <types.h>

/** 32-bit FNV_prime. */
#define FNV_PRIME        16777619UL

/** Defined in the FNV description. Result of hashing a known string with the
 *  FNV-0 algorithm and the above prime. */
#define FNV_OFFSET_BASIS 2166136261UL

/** Iterates over a hash table. */
#define HASH_FOREACH(hash, iter) \
	for(size_t _##iter##_i = 0; _##iter##_i < (hash)->entries; _##iter##_i++) \
		LIST_FOREACH(&(hash)->buckets[_##iter##_i], iter)

/** Iterates over a hash table, safe when modifying the table in the loop. */
#define HASH_FOREACH_SAFE(hash, iter) \
	for(size_t _##iter##_i = 0; _##iter##_i < (hash)->entries; _##iter##_i++) \
		LIST_FOREACH_SAFE(&(hash)->buckets[_##iter##_i], iter)

/** Hash table operations structure. */
typedef struct hash_ops {
	/** Obtains a key for a given entry.
	 * @param entry		Entry to get key of.
	 * @return		Key of entry. */
	key_t (*key)(list_t *entry);

	/** Hashes the given key.
	 * @param key		Key to hash.
	 * @return		Generated hash for key. */
	uint32_t (*hash)(key_t key);

	/** Compares two keys.
	 * @param key1		First key.
	 * @param key2		Second key.
	 * @return		True if match, false if not. */
	bool (*compare)(key_t key1, key_t key2);
} hash_ops_t;

/** Structure containing a hash table. */
typedef struct hash {
	list_t *buckets;		/**< Buckets for the table. */
	size_t entries;			/**< Number of buckets. */
	hash_ops_t *ops;		/**< Hash table operations. */
} hash_t;

/** Generic hash/comparision functions. */
extern uint32_t hash_str_hash(key_t key);
extern bool hash_str_compare(key_t key1, key_t key2);
extern uint32_t hash_int_hash(key_t key);
extern bool hash_int_compare(key_t key1, key_t key2);

/** Main functions. */
extern void hash_insert(hash_t *hash, list_t *entry);
extern bool hash_insert_unique(hash_t *hash, list_t *entry);
extern void hash_remove(list_t *entry);
extern list_t *hash_lookup(hash_t *hash, key_t key);
extern int hash_init(hash_t *hash, size_t entries, hash_ops_t *ops);

#endif /* __TYPES_HASH_H */
