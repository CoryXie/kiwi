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
 * @brief		Futex functions.
 */

#ifndef __KERNEL_FUTEX_H
#define __KERNEL_FUTEX_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern status_t SYSCALL(futex_wait)(int32_t *addr, int32_t val, useconds_t timeout);
extern status_t SYSCALL(futex_wake)(int32_t *addr, size_t count, size_t *wokenp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_FUTEX_H */