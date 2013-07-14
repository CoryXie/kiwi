/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Directory entry cache functions.
 *
 * Implementation notes:
 *  - The radix tree stores pointers to directory entry structures. This allows
 *    the cache to be used to implement read_dir for RamFS.
 *
 * @todo		Could convert to use a rwlock instead of a mutex,
 *			there's no need for exclusive access when reading from
 *			the cache.
 */

#include <io/entry_cache.h>
#include <io/file.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/slab.h>

#include <kernel.h>
#include <status.h>

/** Slab cache for entry cache structures. */
static slab_cache_t *entry_cache_cache = NULL;

/** Entry cache constructor.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void entry_cache_ctor(void *obj, void *data) {
	entry_cache_t *cache = obj;

	mutex_init(&cache->lock, "entry_cache_lock", 0);
	radix_tree_init(&cache->entries);
}

/** Create a new entry cache.
 * @param ops		Pointer to operations structure (optional).
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to created cache. */
entry_cache_t *entry_cache_create(entry_cache_ops_t *ops, void *data) {
	entry_cache_t *cache;

	cache = slab_cache_alloc(entry_cache_cache, MM_KERNEL);
	cache->ops = ops;
	cache->data = data;
	return cache;
}

/** Destroy an entry cache.
 * @param cache		Cache to destroy. */
void entry_cache_destroy(entry_cache_t *cache) {
	radix_tree_clear(&cache->entries, kfree);
	slab_cache_free(entry_cache_cache, cache);
}

/** Insert an entry into an entry cache.
 * @param cache		Cache to insert into (lock held).
 * @param name		Name of entry.
 * @param id		ID of node that entry points to.
 * @return		Pointer to entry structure. */
static dir_entry_t *insert_entry(entry_cache_t *cache, const char *name, node_id_t id) {
	dir_entry_t *entry;
	size_t len;

	len = sizeof(dir_entry_t) + strlen(name) + 1;
	entry = kmalloc(len, MM_KERNEL);
	entry->length = len;
	entry->id = id;
	strcpy(entry->name, name);
	radix_tree_insert(&cache->entries, name, entry);
	return entry;
}

/** Look up an entry in an entry cache.
 * @param cache		Cache to look up in.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry points to.
 * @return		Status code describing result of the operation. */
status_t entry_cache_lookup(entry_cache_t *cache, const char *name, node_id_t *idp) {
	dir_entry_t *entry;
	node_id_t id;
	status_t ret;

	mutex_lock(&cache->lock);

	/* Look up the entry. If it is not found, pull it in. */
	entry = radix_tree_lookup(&cache->entries, name);
	if(!entry) {
		if(!cache->ops || !cache->ops->lookup) {
			mutex_unlock(&cache->lock);
			return STATUS_NOT_FOUND;
		}

		ret = cache->ops->lookup(cache, name, &id);
		if(ret != STATUS_SUCCESS) {
			mutex_unlock(&cache->lock);
			return ret;
		}

		entry = insert_entry(cache, name, id);
	}

	*idp = entry->id;
	mutex_unlock(&cache->lock);
	return STATUS_SUCCESS;
}

/** Insert an entry into an entry cache.
 * @param cache		Cache to insert into.
 * @param name		Name of entry.
 * @param id		ID of node that entry points to. */
void entry_cache_insert(entry_cache_t *cache, const char *name, node_id_t id) {
	mutex_lock(&cache->lock);
	insert_entry(cache, name, id);
	mutex_unlock(&cache->lock);
}

/** Remove an entry from an entry cache.
 * @param cache		Cache to remove from.
 * @param name		Name of entry to remove. */
void entry_cache_remove(entry_cache_t *cache, const char *name) {
	mutex_lock(&cache->lock);
	radix_tree_remove(&cache->entries, name, kfree);
	mutex_unlock(&cache->lock);
}

/** Initialize the entry cache slab cache. */
static __init_text void entry_cache_init(void) {
	entry_cache_cache = slab_cache_create("entry_cache_cache",
		sizeof(entry_cache_t), 0, entry_cache_ctor, NULL,
		NULL, 0, MM_BOOT);
}

INITCALL(entry_cache_init);
