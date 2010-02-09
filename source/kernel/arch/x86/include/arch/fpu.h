/*
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
 * @brief		x86 FPU functions.
 */

#ifndef __ARCH_FPU_H
#define __ARCH_FPU_H

/** FPU context structure alignment. */
#define FPU_CONTEXT_ALIGN	16

/** Structure containing an x86 FPU context. */
typedef struct fpu_context {
	char data[512];
} fpu_context_t;

#endif /* __ARCH_FPU_H */