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
 * @brief		Process management functions.
 */

#ifndef __KERNEL_PROCESS_H
#define __KERNEL_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

/** Arguments passed to the userspace loader. */
typedef struct process_args {
	char *path;			/**< Path to program. */
	char **args;			/**< Argument array. */
	char **env;			/**< Environment variable array. */
	int args_count;			/**< Number of entries in argument array (excluding NULL). */
	int env_count;			/**< Number of entries in environment array (excluding NULL). */
} process_args_t;

/** Expected path to libkernel. */
#define LIBKERNEL_PATH		"/system/libraries/libkernel.so"

/** Process object events. */
#define PROCESS_EVENT_DEATH	0	/**< Wait for process death. */

extern status_t SYSCALL(process_create)(const char *path, const char *const args[],
                                        const char *const env[], int flags,
                                        handle_t map[][2], int count,
                                        handle_t *handlep);
extern status_t SYSCALL(process_replace)(const char *path, const char *const args[],
                                         const char *const env[], handle_t map[][2],
                                         int count);
extern status_t SYSCALL(process_open)(process_id_t id, handle_t *handlep);
extern process_id_t SYSCALL(process_id)(handle_t handle);
extern status_t SYSCALL(process_status)(handle_t handle, int *statusp);
extern void SYSCALL(process_exit)(int status) __attribute__((noreturn));
extern void SYSCALL(process_loaded)(void);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PROCESS_H */
