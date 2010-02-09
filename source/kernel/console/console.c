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
 * @brief		Kernel console functions.
 */

#include <arch/page.h>

#include <lib/printf.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kdbg.h>
#include <types.h>

/** List of kernel consoles. */
static LIST_DECLARE(console_list);
static SPINLOCK_DECLARE(console_lock);

/** Cyclic kernel log buffer. Defined as a structure to make access easier. */
static struct {
	unsigned char level;		/**< Log level. */
	unsigned char ch;		/**< Character. */
} klog_buffer[CONFIG_KLOG_SIZE] __aligned(PAGE_SIZE);

/** Start of the log buffer. */
static uint32_t klog_start = 0;

/** Number of characters in the buffer. */
static uint32_t klog_length = 0;

/** Write a character to the console/log
 * @note		Does not take the console lock.
 * @param level		Log level.
 * @param ch		Character to print. */
static void console_putch_unlocked(int level, char ch) {
	console_t *cons;
#if !CONFIG_DEBUG
	if(level == LOG_DEBUG) {
		return;
	}
#endif
	LIST_FOREACH(&console_list, iter) {
		cons = list_entry(iter, console_t, header);

		if(level < cons->min_level) {
			continue;
		} else if(unlikely(!cons->putch)) {
			continue;
		}

		cons->putch((unsigned char)ch);
	}

	/* Store in the log buffer. */
	if(level != LOG_NONE) {
		klog_buffer[(klog_start + klog_length) % CONFIG_KLOG_SIZE].level = level;
		klog_buffer[(klog_start + klog_length) % CONFIG_KLOG_SIZE].ch = (unsigned char)ch;
		if(klog_length < CONFIG_KLOG_SIZE) {
			klog_length++;
		} else {
			klog_start = (klog_start + 1) % CONFIG_KLOG_SIZE;
		}
	}
}

/** Helper for kvprintf().
 * @param ch		Character to display.
 * @param data		Pointer to log level.
 * @param total		Pointer to total character count. */
static void kvprintf_helper(char ch, void *data, int *total) {
	int level = *(int *)data;
	console_putch_unlocked(level, ch);
	*total = *total + 1;
}

/** Write a character to the console.
 *
 * Writes a character to all currently registered consoles that allow the
 * specified log level, and stores it in the kernel log buffer.
 *
 * @param level		Log level.
 * @param ch		Character to print.
 */
void console_putch(int level, char ch) {
	if(level != LOG_NONE) {
		spinlock_lock(&console_lock, 0);
		console_putch_unlocked(level, ch);
		spinlock_unlock(&console_lock);
	} else {
		console_putch_unlocked(level, ch);
	}
}

/** Register a console and initialise it.
 * @param cons		Console to register. */
void console_register(console_t *cons) {
	spinlock_lock(&console_lock, 0);

	cons->inhibited = false;
	list_init(&cons->header);
	list_append(&console_list, &cons->header);

	if(cons->init) {
		cons->init();
	}

	spinlock_unlock(&console_lock);
}

/** Unregister a console.
 * @param cons		Console to register. */
void console_unregister(console_t *cons) {
	spinlock_lock(&console_lock, 0);
	list_remove(&cons->header);
	spinlock_unlock(&console_lock);
}

/** Print a formatted message to the kernel log.
 * @param level		Log level.
 * @param fmt		Format string for message.
 * @param args		Arguments to substitute into format string.
 * @return		Number of characters written. */
int kvprintf(int level, const char *fmt, va_list args) {
	int ret;
#if !CONFIG_DEBUG
	/* When debug output is disabled, do not do anything. */
	if(level == LOG_DEBUG) {
		return 0;
	}
#endif
	/* Do not take the lock if not logging as LOG_NONE is usually used from
	 * KDBG/fatal(). */
	if(level != LOG_NONE) {
		spinlock_lock(&console_lock, 0);
		ret = do_printf(kvprintf_helper, &level, fmt, args);
		spinlock_unlock(&console_lock);
	} else {
		ret = do_printf(kvprintf_helper, &level, fmt, args);
	}

	return ret;
}

/** Print a formatted message to the kernel log.
 * @param level		Log level.
 * @param fmt		Format string for message.
 * @param ...		Arguments to substitute into format string.
 * @return		Number of characters written. */
int kprintf(int level, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = kvprintf(level, fmt, args);
	va_end(args);

	return ret;
}

/** Print out the kernel log buffer.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		Command status. */
int kdbg_cmd_log(int argc, char **argv) {
	int level = -1;
	size_t i, pos;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [/level]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out the contents of the kernel log buffer. If no level is specified\n");
		kprintf(LOG_NONE, "the entire log will be printed, otherwise only characters with the specified\n");
		kprintf(LOG_NONE, "level or higher will be printed.\n");
		kprintf(LOG_NONE, "  Log levels:\n");
		kprintf(LOG_NONE, "    d    Debug.\n");
		kprintf(LOG_NONE, "    n    Normal.\n");
		kprintf(LOG_NONE, "    w    Warning.\n");
		return KDBG_OK;
	} else if(!(argc == 1 || (argc == 2 && argv[1][0] == '/'))) {
		kprintf(LOG_NONE, "Invalid arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Look for a log level. */
	if(argc == 2) {
		argv[1]++;
		switch(*argv[1]) {
		case 'd': level = LOG_DEBUG; break;
		case 'n': level = LOG_NORMAL; break;
		case 'w': level = LOG_WARN; break;
		default:
			kprintf(LOG_NONE, "Unknown level character '%c'\n", *argv[1]);
			return KDBG_FAIL;
		}
	}

	for(i = 0, pos = klog_start; i < klog_length; i++) {
		if(level == -1 || klog_buffer[pos].level >= level) {
			console_putch(LOG_NONE, klog_buffer[pos].ch);
		}
		if(++pos >= CONFIG_KLOG_SIZE) {
			pos = 0;
		}
	}
	return KDBG_OK;
}