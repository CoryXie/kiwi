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
 * @brief		Directory entry cache functions.
 *
 * Implementation notes:
 *  - The radix tree stores pointers to directory entry structures. This allows
 *    the cache to be used to implement read_entry for RamFS.
 */

#include <io/entry_cache.h>
#include <io/fs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/slab.h>

#include <errors.h>
#include <fatal.h>
#include <init.h>

/** Slab cache for entry cache structures. */
static slab_cache_t *entry_cache_cache;

/** Entry cache constructor.
 * @param obj		Object to construct.
 * @param data		Unused.
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int entry_cache_ctor(void *obj, void *data, int kmflag) {
	entry_cache_t *cache = obj;

	mutex_init(&cache->lock, "entry_cache_lock", 0);
	radix_tree_init(&cache->entries);
	return 0;
}

/** Create a new entry cache.
 * @param ops		Pointer to operations structure (optional).
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to created cache. */
entry_cache_t *entry_cache_create(entry_cache_ops_t *ops, void *data) {
	entry_cache_t *cache;

	cache = slab_cache_alloc(entry_cache_cache, MM_SLEEP);
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
static fs_dir_entry_t *entry_cache_insert_internal(entry_cache_t *cache, const char *name, node_id_t id) {
	fs_dir_entry_t *entry;
	size_t len;

	len = sizeof(fs_dir_entry_t) + strlen(name) + 1;
	entry = kmalloc(len, MM_SLEEP);
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
 * @return		0 on success, negative error code on failure. */
int entry_cache_lookup(entry_cache_t *cache, const char *name, node_id_t *idp) {
	fs_dir_entry_t *entry;
	node_id_t id;
	int ret;

	mutex_lock(&cache->lock);

	/* Look up the entry. If it is not found, pull it in. */
	if(!(entry = radix_tree_lookup(&cache->entries, name))) {
		if(!cache->ops || !cache->ops->lookup) {
			mutex_unlock(&cache->lock);
			return -ERR_NOT_FOUND;
		} else if((ret = cache->ops->lookup(cache, name, &id)) != 0) {
			mutex_unlock(&cache->lock);
			return ret;
		}

		entry = entry_cache_insert_internal(cache, name, id);
	}

	*idp = entry->id;
	mutex_unlock(&cache->lock);
	return 0;
}

/** Insert an entry into an entry cache.
 * @param cache		Cache to insert into.
 * @param name		Name of entry.
 * @param id		ID of node that entry points to. */
void entry_cache_insert(entry_cache_t *cache, const char *name, node_id_t id) {
	mutex_lock(&cache->lock);
	entry_cache_insert_internal(cache, name, id);
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

/** Initialise the entry cache slab cache. */
static void __init_text entry_cache_init(void) {
	entry_cache_cache = slab_cache_create("entry_cache_cache", sizeof(entry_cache_t),
	                                      0, entry_cache_ctor, NULL, NULL, NULL,
	                                      SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
INITCALL(entry_cache_init);