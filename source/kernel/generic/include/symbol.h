/* Kiwi symbol table manager
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
 * @brief		Symbol table manager.
 */

#ifndef __SYMTAB_H
#define __SYMTAB_H

#include <types.h>

/** Information about a symbol in the kernel or a module. */
typedef struct symbol {
	ptr_t addr;		/**< Address that the symbol points to. */
	size_t size;		/**< Size of symbol. */
	const char *name;	/**< Name of the symbol. */
	bool global;		/**< Whether the symbol is global. */
	bool exported;		/**< Whether the symbol has been exported for modules to link to. */
} symbol_t;

/** Structure containing a symbol table. */
typedef struct symbol_table {
	symbol_t *symbols;	/**< Array of symbols. */
	size_t count;		/**< Number of symbols in the table. */
} symbol_table_t;

/** Kernel symbol table. */
extern symbol_table_t kernel_symtab;

/** Symbol table functions. */
extern symbol_t *symbol_table_lookup_addr(symbol_table_t *table, ptr_t addr, size_t *offp);
extern symbol_t *symbol_table_lookup_name(symbol_table_t *table, const char *name, bool global, bool exported);
extern void symbol_table_init(symbol_table_t *table);
extern void symbol_table_destroy(symbol_table_t *table);
extern void symbol_table_insert(symbol_table_t *table, const char *name, ptr_t addr,
                                size_t size, bool global, bool exported);

/** Kernel symbol lookup functions. */
extern symbol_t *symbol_lookup_addr(ptr_t addr, size_t *offp);
extern symbol_t *symbol_lookup_name(const char *name, bool global, bool exported);

#endif /* __SYMTAB_H */
